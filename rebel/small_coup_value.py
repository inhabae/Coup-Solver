#!/usr/bin/env python3
"""Train a compact value model from the pybind11 small_coup ReBeL bridge."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Iterable

try:
    import torch
    from torch import nn
    from torch.utils.data import DataLoader, Dataset, random_split
except ModuleNotFoundError:
    torch = None
    nn = None
    DataLoader = None
    Dataset = object
    random_split = None

try:
    import _small_coup_rebel as rebel
except ModuleNotFoundError:
    rebel = None


SCHEMA = "small_coup_rebel_pybind_v2"


def require_torch() -> None:
    if torch is None:
        raise RuntimeError("PyTorch is not installed. Install rebel/requirements.txt in .venv.")


def require_bridge() -> None:
    if rebel is None:
        raise RuntimeError("pybind11 bridge is not built. Run: cd rebel && ../.venv/bin/python setup.py build_ext --inplace")


class ValueDataset(Dataset):
    def __init__(self, samples: Iterable[object]) -> None:
        require_torch()
        materialized = list(samples)
        if not materialized:
            raise ValueError("no samples generated")
        self.features = torch.tensor([sample.features() for sample in materialized], dtype=torch.float32)
        self.values = torch.tensor([[float(sample.target_value)] for sample in materialized], dtype=torch.float32)

    def __len__(self) -> int:
        return int(self.features.shape[0])

    def __getitem__(self, index: int) -> tuple[torch.Tensor, torch.Tensor]:
        return self.features[index], self.values[index]


class ValueNet(nn.Module if nn is not None else object):
    def __init__(self, input_dim: int, hidden_dim: int = 64) -> None:
        require_torch()
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(input_dim, hidden_dim),
            nn.ReLU(),
            nn.Linear(hidden_dim, hidden_dim),
            nn.ReLU(),
            nn.Linear(hidden_dim, 1),
            nn.Tanh(),
        )

    def forward(self, features: torch.Tensor) -> torch.Tensor:
        return self.net(features)


class TorchValueEvaluator:
    def __init__(self, checkpoint_path: Path) -> None:
        require_torch()
        require_bridge()
        checkpoint = torch.load(checkpoint_path, map_location="cpu")
        if checkpoint.get("schema") != SCHEMA:
            raise ValueError(f"checkpoint schema mismatch: {checkpoint.get('schema')!r}")
        self.feature_dim = int(checkpoint["feature_dim"])
        self.model = ValueNet(input_dim=self.feature_dim, hidden_dim=int(checkpoint["hidden_dim"]))
        self.model.load_state_dict(checkpoint["model_state_dict"])
        self.model.eval()

    def value(self, public_state: object, belief: object, player: int) -> float:
        features = rebel.value_features(public_state, belief, int(player))
        if len(features) != self.feature_dim:
            raise ValueError(f"feature dimension mismatch: got {len(features)}, expected {self.feature_dim}")
        with torch.no_grad():
            tensor = torch.tensor([features], dtype=torch.float32)
            return float(self.model(tensor).item())


class TorchModelEvaluator:
    def __init__(self, model: object, feature_dim: int) -> None:
        require_torch()
        require_bridge()
        self.model = model
        self.feature_dim = int(feature_dim)

    def value(self, public_state: object, belief: object, player: int) -> float:
        features = rebel.value_features(public_state, belief, int(player))
        if len(features) != self.feature_dim:
            raise ValueError(f"feature dimension mismatch: got {len(features)}, expected {self.feature_dim}")
        was_training = bool(self.model.training)
        self.model.eval()
        try:
            with torch.no_grad():
                tensor = torch.tensor([features], dtype=torch.float32)
                return float(self.model(tensor).item())
        finally:
            if was_training:
                self.model.train()


def wrap_pybind_evaluator(torch_evaluator: object) -> object:
    require_bridge()

    class ModelBackedEvaluator(rebel.ValueEvaluator):
        def __init__(self) -> None:
            super().__init__()

        def evaluate(self, public_state: object, belief: object, player: int) -> float:
            return torch_evaluator.value(public_state, belief, player)

    evaluator = ModelBackedEvaluator()
    evaluator.torch_evaluator = torch_evaluator
    return evaluator


def make_pybind_evaluator(checkpoint_path: Path) -> object:
    return wrap_pybind_evaluator(TorchValueEvaluator(checkpoint_path))


def make_model_pybind_evaluator(model: object, feature_dim: int) -> object:
    return wrap_pybind_evaluator(TorchModelEvaluator(model, feature_dim))


def infer_feature_dim() -> int:
    require_bridge()
    state = rebel.GameState(rebel.all_deals()[0])
    public_state = rebel.public_state_from(state)
    belief = rebel.belief_from_public_state(public_state)
    return len(rebel.value_features(public_state, belief, 0))


def generate_samples(args: argparse.Namespace) -> list[object]:
    require_bridge()
    if getattr(args, "bootstrap_heuristic", False):
        return rebel.generate_training_samples(
            args.samples,
            args.max_steps,
            args.seed,
            args.resolve_iterations,
            args.resolve_depth,
        )
    if getattr(args, "leaf_checkpoint", None):
        evaluator = make_pybind_evaluator(Path(args.leaf_checkpoint))
        return rebel.generate_training_samples_with_evaluator(
            args.samples,
            args.max_steps,
            args.seed,
            args.resolve_iterations,
            args.resolve_depth,
            evaluator,
        )
    require_torch()
    torch.manual_seed(args.seed)
    feature_dim = infer_feature_dim()
    model = ValueNet(input_dim=feature_dim, hidden_dim=args.hidden_dim)
    evaluator = make_model_pybind_evaluator(model, feature_dim)
    return rebel.generate_training_samples_with_evaluator(
        args.samples,
        args.max_steps,
        args.seed,
        args.resolve_iterations,
        args.resolve_depth,
        evaluator,
    )


def train(args: argparse.Namespace) -> None:
    require_torch()
    require_bridge()
    if args.rebel_iterations <= 0:
        raise ValueError("--rebel-iterations must be positive")
    torch.manual_seed(args.seed)
    feature_dim = infer_feature_dim()
    model = ValueNet(input_dim=feature_dim, hidden_dim=args.hidden_dim)
    if args.checkpoint:
        checkpoint = torch.load(Path(args.checkpoint), map_location="cpu")
        if checkpoint.get("schema") != SCHEMA:
            raise ValueError(f"checkpoint schema mismatch: {checkpoint.get('schema')!r}")
        if int(checkpoint["feature_dim"]) != feature_dim:
            raise ValueError(f"checkpoint feature dimension mismatch: {checkpoint['feature_dim']} != {feature_dim}")
        if int(checkpoint["hidden_dim"]) != args.hidden_dim:
            raise ValueError(f"checkpoint hidden dimension mismatch: {checkpoint['hidden_dim']} != {args.hidden_dim}")
        model.load_state_dict(checkpoint["model_state_dict"])
    optimizer = torch.optim.AdamW(model.parameters(), lr=args.learning_rate, weight_decay=args.weight_decay)
    loss_fn = nn.MSELoss()
    best_val = float("inf")
    best_state = None

    total_samples = 0
    for rebel_iteration in range(1, args.rebel_iterations + 1):
        evaluator = make_model_pybind_evaluator(model, feature_dim)
        samples = rebel.generate_training_samples_with_evaluator(
            args.samples,
            args.max_steps,
            args.seed + rebel_iteration - 1,
            args.resolve_iterations,
            args.resolve_depth,
            evaluator,
        )
        total_samples += len(samples)
        dataset = ValueDataset(samples)
        val_size = max(1, int(len(dataset) * args.validation_fraction)) if len(dataset) > 1 else 0
        train_size = len(dataset) - val_size
        train_dataset, val_dataset = random_split(
            dataset,
            [train_size, val_size],
            generator=torch.Generator().manual_seed(args.seed + rebel_iteration - 1),
        )
        train_loader = DataLoader(train_dataset, batch_size=args.batch_size, shuffle=True)
        val_loader = DataLoader(val_dataset, batch_size=args.batch_size) if val_size else None

        for epoch in range(1, args.epochs + 1):
            model.train()
            train_loss = 0.0
            train_count = 0
            for features, values in train_loader:
                optimizer.zero_grad(set_to_none=True)
                loss = loss_fn(model(features), values)
                loss.backward()
                optimizer.step()
                train_loss += float(loss.item()) * int(features.shape[0])
                train_count += int(features.shape[0])

            model.eval()
            val_loss = 0.0
            val_count = 0
            if val_loader is not None:
                with torch.no_grad():
                    for features, values in val_loader:
                        loss = loss_fn(model(features), values)
                        val_loss += float(loss.item()) * int(features.shape[0])
                        val_count += int(features.shape[0])

            print(
                json.dumps(
                    {
                        "rebel_iteration": rebel_iteration,
                        "epoch": epoch,
                        "leaf_evaluator": "value_model",
                        "train_mse": train_loss / max(train_count, 1),
                        "val_mse": val_loss / max(val_count, 1) if val_count else None,
                        "samples": len(dataset),
                    }
                )
            )

            if val_count:
                current_val = val_loss / val_count
                if best_state is None or current_val < best_val:
                    best_val = current_val
                    best_state = {key: value.detach().clone() for key, value in model.state_dict().items()}

    if best_state is not None:
        model.load_state_dict(best_state)

    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    torch.save(
        {
            "schema": SCHEMA,
            "feature_dim": feature_dim,
            "hidden_dim": args.hidden_dim,
            "model_state_dict": model.state_dict(),
            "metadata": {
                "epochs": args.epochs,
                "rebel_iterations": args.rebel_iterations,
                "seed": args.seed,
                "samples": total_samples,
                "max_steps": args.max_steps,
                "resolve_iterations": args.resolve_iterations,
                "resolve_depth": args.resolve_depth,
                "leaf_evaluator": "value_model",
                "best_val_mse": best_val if best_state is not None else None,
            },
        },
        output,
    )


def inspect(args: argparse.Namespace) -> None:
    samples = generate_samples(args)
    first = samples[0]
    print(
        json.dumps(
            {
                "schema": SCHEMA,
                "samples": len(samples),
                "feature_dim": len(first.features()),
                "first_public_key": first.public_state.serialize(),
                "first_policy_sum": sum(first.search_result.policy),
            }
        )
    )


def compare_model(args: argparse.Namespace) -> None:
    require_bridge()
    evaluator = make_pybind_evaluator(Path(args.checkpoint))
    deals = rebel.all_deals()
    state = rebel.GameState(deals[args.deal_index % len(deals)])
    for action_name in args.actions:
        action = getattr(rebel.Action, action_name)
        if not state.is_legal(action):
            raise ValueError(f"illegal scripted action {action_name} at state:\n{state.debug_string()}")
        state.apply(action)

    player = state.current_player()
    heuristic = rebel.resolve_heuristic(state, player, args.resolve_iterations, args.resolve_depth, args.seed)
    model = rebel.resolve_with_evaluator(state, evaluator, player, args.resolve_iterations, args.resolve_depth, args.seed)
    public_state = rebel.public_state_from(state)
    belief = rebel.belief_from_public_state(public_state)
    print(
        json.dumps(
            {
                "public_key": public_state.serialize(),
                "player": player,
                "model_leaf_value": evaluator.evaluate(public_state, belief, player),
                "heuristic_resolve_value": heuristic.value,
                "model_resolve_value": model.value,
                "heuristic_policy": heuristic.policy,
                "model_policy": model.policy,
            }
        )
    )


def self_test() -> None:
    require_torch()
    require_bridge()
    feature_dim = infer_feature_dim()
    model = ValueNet(input_dim=feature_dim, hidden_dim=8)
    evaluator = make_model_pybind_evaluator(model, feature_dim)
    samples = rebel.generate_training_samples_with_evaluator(4, 8, 1, 8, 2, evaluator)
    dataset = ValueDataset(samples)
    model = ValueNet(input_dim=int(dataset.features.shape[1]), hidden_dim=8)
    output = model(dataset.features)
    assert tuple(output.shape) == (4, 1)
    assert torch.all(output <= 1.0)
    assert torch.all(output >= -1.0)


def bridge_callback_self_test() -> None:
    require_bridge()

    class ConstantEvaluator(rebel.ValueEvaluator):
        def __init__(self) -> None:
            super().__init__()

        def evaluate(self, public_state: object, belief: object, player: int) -> float:
            return 0.25 if player == 0 else -0.25

    state = rebel.GameState(rebel.all_deals()[0])
    result = rebel.resolve_with_evaluator(state, ConstantEvaluator(), state.current_player(), 8, 2, 1)
    assert abs(sum(result.policy) - 1.0) < 1e-9


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", default="rebel_artifacts/small_coup_value.pt")
    parser.add_argument("--checkpoint", help="checkpoint for --compare-model or to initialize training")
    parser.add_argument("--leaf-checkpoint", help="explicit checkpoint leaf evaluator for --inspect")
    parser.add_argument("--samples", type=int, default=1000)
    parser.add_argument("--max-steps", type=int, default=16)
    parser.add_argument("--resolve-iterations", type=int, default=64)
    parser.add_argument("--resolve-depth", type=int, default=4)
    parser.add_argument("--epochs", type=int, default=20)
    parser.add_argument("--rebel-iterations", type=int, default=1)
    parser.add_argument("--batch-size", type=int, default=64)
    parser.add_argument("--hidden-dim", type=int, default=64)
    parser.add_argument("--learning-rate", type=float, default=1e-3)
    parser.add_argument("--weight-decay", type=float, default=1e-4)
    parser.add_argument("--validation-fraction", type=float, default=0.2)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--inspect", action="store_true")
    parser.add_argument("--bootstrap-heuristic", action="store_true", help="use heuristic leaves for inspection/bootstrap only")
    parser.add_argument("--compare-model", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--bridge-callback-self-test", action="store_true")
    parser.add_argument("--deal-index", type=int, default=0)
    parser.add_argument("--actions", nargs="*", default=[])
    args = parser.parse_args()
    if args.bridge_callback_self_test:
        bridge_callback_self_test()
        return
    if args.self_test:
        self_test()
        return
    if args.compare_model:
        if not args.checkpoint:
            parser.error("--checkpoint is required with --compare-model")
        compare_model(args)
        return
    if args.inspect:
        inspect(args)
        return
    train(args)


if __name__ == "__main__":
    main()
