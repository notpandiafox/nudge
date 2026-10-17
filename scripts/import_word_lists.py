"""Downloads the two Wordle word lists at pinned commits, checks them, and
writes data/answers.inc and data/guesses.inc for the solver to compile in.

Run by hand only when updating the lists (plan §6.1):
    python scripts/import_word_lists.py
"""

import collections
import datetime
import json
import re
import urllib.request
from pathlib import Path
from typing import NamedTuple


class Source(NamedTuple):
    repo: str
    commit: str
    path: str

    def raw_url(self) -> str:
        return f"https://raw.githubusercontent.com/{self.repo}/{self.commit}/{self.path}"


# Pinned commits: updating a list is a deliberate change to one of these lines.
ANSWERS = Source("stuartpb/wordles", "6072f5b335dfa33b6abfb8ab18fbce2ba42f2b62", "wordles.json")
GUESSES = Source("tabatkins/wordle-list", "255b9469c4dad99a3b95cc4ddbe139b3d3747868", "words")

DATA_DIR = Path(__file__).resolve().parent.parent / "data"


def download(source: Source) -> str:  # pragma: no cover (needs the network)
    with urllib.request.urlopen(source.raw_url(), timeout=30) as response:
        return response.read().decode("utf-8")


def parse_answers(text: str) -> list[str]:
    return json.loads(text)


def parse_guesses(text: str) -> list[str]:
    return text.split()


def prepare(answers: list[str], guesses: list[str]) -> tuple[list[str], list[str]]:
    """Lowercases and sorts both lists; raises ValueError if they can't be used."""
    answers = sorted(word.lower() for word in answers)
    guesses = sorted(word.lower() for word in guesses)
    for name, words in (("answers", answers), ("guesses", guesses)):
        bad = [word for word in words if not re.fullmatch("[a-z]{5}", word)]
        if bad:
            raise ValueError(f"{name}: not 5 letters a-z: {', '.join(bad)}")
        duplicates = [word for word, count in collections.Counter(words).items() if count > 1]
        if duplicates:
            raise ValueError(f"duplicates in {name}: {', '.join(duplicates)}")
    missing = sorted(set(answers) - set(guesses))
    if missing:
        raise ValueError(f"answers missing from the guess list: {', '.join(missing)}")
    return answers, guesses


def to_inc(source: Source, words: list[str], retrieved: datetime.date) -> str:
    header = (
        f"// source: https://github.com/{source.repo} (file {source.path})\n"
        f"// commit: {source.commit}   retrieved: {retrieved.isoformat()}\n"
    )
    return header + "".join(f'"{word}",\n' for word in words)


def main(data_dir: Path = DATA_DIR, retrieved: datetime.date | None = None) -> None:
    retrieved = retrieved or datetime.date.today()
    answers, guesses = prepare(
        parse_answers(download(ANSWERS)), parse_guesses(download(GUESSES))
    )
    data_dir.mkdir(parents=True, exist_ok=True)
    for name, source, words in (("answers", ANSWERS, answers), ("guesses", GUESSES, guesses)):
        # newline="\n": LF on every system, so the files are byte-identical everywhere.
        (data_dir / f"{name}.inc").write_text(
            to_inc(source, words, retrieved), encoding="utf-8", newline="\n"
        )
    print(f"wrote {len(answers)} answers and {len(guesses)} guesses to {data_dir}")


if __name__ == "__main__":
    main()
