import datetime
import json
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import import_word_lists as iwl  # noqa: E402


def test_prepare_lowercases_and_sorts():
    answers, guesses = iwl.prepare(["Crane"], ["slate", "CRANE", "abbey"])
    assert answers == ["crane"]
    assert guesses == ["abbey", "crane", "slate"]


@pytest.mark.parametrize(
    "answers, guesses, message",
    [
        (["cranes"], ["cranes"], "not 5 letters a-z: cranes"),
        (["cr4ne"], ["cr4ne"], "not 5 letters a-z: cr4ne"),
        (["crane"], ["crane", "slate", "CRANE"], "duplicates in guesses: crane"),
        (["crane", "crane"], ["crane"], "duplicates in answers: crane"),
        (["crane", "slate"], ["crane"], "answers missing from the guess list: slate"),
    ],
)
def test_prepare_rejects_bad_lists(answers, guesses, message):
    with pytest.raises(ValueError, match=message):
        iwl.prepare(answers, guesses)


def test_raw_url_downloads_the_pinned_commit_not_the_latest():
    source = iwl.Source("someone/words", "abc123", "words.txt")
    assert source.raw_url() == "https://raw.githubusercontent.com/someone/words/abc123/words.txt"


def test_parse_answers_reads_the_json_list():
    assert iwl.parse_answers(json.dumps(["cigar", "rebut"])) == ["cigar", "rebut"]


def test_parse_guesses_reads_one_word_per_line_with_any_line_endings():
    assert iwl.parse_guesses("crane\r\nslate\r\n\nabbey\n") == ["crane", "slate", "abbey"]


def test_to_inc_writes_source_comments_then_one_cpp_string_per_line():
    source = iwl.Source("someone/words", "abc123", "words.txt")
    text = iwl.to_inc(source, ["abbey", "crane"], datetime.date(2026, 10, 17))
    assert text == (
        "// source: https://github.com/someone/words (file words.txt)\n"
        "// commit: abc123   retrieved: 2026-10-17\n"
        '"abbey",\n'
        '"crane",\n'
    )


def test_main_downloads_checks_and_writes_both_files_with_lf(tmp_path, monkeypatch):
    texts = {
        iwl.ANSWERS: json.dumps(["Crane"]),
        iwl.GUESSES: "slate\r\ncrane\r\n",
    }
    monkeypatch.setattr(iwl, "download", texts.__getitem__)

    data_dir = tmp_path / "data"  # doesn't exist yet, like data/ on the first run
    iwl.main(data_dir, datetime.date(2026, 10, 17))

    answers = (data_dir / "answers.inc").read_bytes()
    guesses = (data_dir / "guesses.inc").read_bytes()
    assert b"\r" not in answers + guesses
    assert answers.endswith(b'"crane",\n')
    assert guesses.endswith(b'"crane",\n"slate",\n')
    assert iwl.ANSWERS.commit.encode() in answers
