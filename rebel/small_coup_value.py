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


SCHEMA = "small_coup_rebel_pybind_v1"


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


def generate_samples(args: argparse.Namespace) -> list[object]:
    require_bridge()
    return rebel.generate_training_samples(
        args.samples,
        args.max_steps,
        args.seed,
        args.resolve_iterations,
        args.resolve_depth,
    )


def train(args: argparse.Namespace) -> None:
    require_torch()
    torch.manual_seed(args.seed)
    samples = generate_samples(args)
    dataset = ValueDataset(samples)
    val_size = max(1, int(len(dataset) * args.validation_fraction)) if len(dataset) > 1 else 0
    train_size = len(dataset) - val_size
    train_dataset, val_dataset = random_split(
        dataset,
        [train_size, val_size],
        generator=torch.Generator().manual_seed(args.seed),
    )
    train_loader = DataLoader(train_dataset, batch_size=args.batch_size, shuffle=True)
    val_loader = DataLoader(val_dataset, batch_size=args.batch_size) if val_size else None

    model = ValueNet(input_dim=int(dataset.features.shape[1]), hidden_dim=args.hidden_dim)
    optimizer = torch.optim.AdamW(model.parameters(), lr=args.learning_rate, weight_decay=args.weight_decay)
    loss_fn = nn.MSELoss()

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
                    "epoch": epoch,
                    "train_mse": train_loss / max(train_count, 1),
                    "val_mse": val_loss / max(val_count, 1) if val_count else None,
                    "samples": len(dataset),
                }
            )
        )

    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    torch.save(
        {
            "schema": SCHEMA,
            "feature_dim": int(dataset.features.shape[1]),
            "hidden_dim": args.hidden_dim,
            "model_state_dict": model.state_dict(),
            "metadata": {
                "epochs": args.epochs,
                "seed": args.seed,
                "samples": len(samples),
                "max_steps": args.max_steps,
                "resolve_iterations": args.resolve_iterations,
                "resolve_depth": args.resolve_depth,
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


def self_test() -> None:
    require_torch()
    require_bridge()
    samples = rebel.generate_training_samples(4, 8, 1, 8, 2)
    dataset = ValueDataset(samples)
    model = ValueNet(input_dim=int(dataset.features.shape[1]), hidden_dim=8)
    output = model(dataset.features)
    assert tuple(output.shape) == (4, 1)
    assert torch.all(output <= 1.0)
    assert torch.all(output >= -1.0)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", default="rebel_artifacts/small_coup_value.pt")
    parser.add_argument("--samples", type=int, default=1000)
    parser.add_argument("--max-steps", type=int, default=16)
    parser.add_argument("--resolve-iterations", type=int, default=64)
    parser.add_argument("--resolve-depth", type=int, default=4)
    parser.add_argument("--epochs", type=int, default=20)
    parser.add_argument("--batch-size", type=int, default=64)
    parser.add_argument("--hidden-dim", type=int, default=64)
    parser.add_argument("--learning-rate", type=float, default=1e-3)
    parser.add_argument("--weight-decay", type=float, default=1e-4)
    parser.add_argument("--validation-fraction", type=float, default=0.2)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--inspect", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    if args.inspect:
        inspect(args)
        return
    train(args)


if __name__ == "__main__":
    main()
