"""Check tracked documentation, configuration and local Markdown links."""

import json
import re
import subprocess
import sys
from pathlib import Path
from urllib.parse import unquote, urlsplit

import yaml


ROOT = Path(__file__).resolve().parent.parent
TEXT_SUFFIXES = {".md", ".yml", ".yaml", ".json", ".py"}
TEXT_NAMES = {".editorconfig", ".gitattributes", ".gitignore", "requirements-dev.txt"}


def unique_pairs(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate key: {key!r}")
        result[key] = value
    return result


class UniqueLoader(yaml.SafeLoader):
    """Safe YAML parsing that also rejects accidentally repeated keys."""


def unique_mapping(loader, node):
    loader.flatten_mapping(node)
    return unique_pairs(
        (loader.construct_object(key), loader.construct_object(value))
        for key, value in node.value
    )


UniqueLoader.add_constructor(
    yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG, unique_mapping
)


def local_link_errors(path, content):
    # Check ordinary inline links outside fenced code. Anchors and reference-style
    # links are intentionally outside this small check's scope.
    fence = None
    for number, line in enumerate(content.splitlines(), 1):
        marker = re.match(r"^\s*(`{3,}|~{3,})", line)
        if marker:
            token = marker[1]
            if fence is None:
                fence = token
            elif token[0] == fence[0] and len(token) >= len(fence):
                fence = None
            continue
        if fence:
            continue
        for match in re.finditer(r"\[[^\]\n]*\]\((<[^>]+>|[^\s)]+)(?:\s+\"[^\"]*\")?\)", line):
            target = match[1].strip("<>")
            url = urlsplit(target)
            if url.scheme or url.netloc or not url.path:
                continue
            destination = unquote(url.path)
            resolved = (ROOT / destination.lstrip("/") if destination.startswith("/")
                        else path.parent / destination).resolve()
            if not resolved.is_relative_to(ROOT) or not resolved.exists():
                yield f"line {number}: missing local link target: {target}"


def main():
    tracked = subprocess.check_output(
        ["git", "ls-files", "-z"], cwd=ROOT
    ).decode("utf-8").split("\0")
    errors = []
    checked = 0
    for name in filter(None, tracked):
        path = ROOT / name
        if path.suffix not in TEXT_SUFFIXES and path.name not in TEXT_NAMES:
            continue
        if path.is_symlink():
            continue
        checked += 1
        try:
            content = path.read_bytes().decode("utf-8")
            if "\r" in content:
                errors.append(f"{name}: use LF line endings")
            if content and not content.endswith("\n"):
                errors.append(f"{name}: missing final newline")
            for number, line in enumerate(content.splitlines(), 1):
                if line.rstrip(" \t") != line:
                    errors.append(f"{name}:{number}: trailing whitespace")
            if path.suffix in {".yaml", ".yml"}:
                list(yaml.load_all(content, Loader=UniqueLoader))
            elif path.suffix == ".json":
                json.loads(content, object_pairs_hook=unique_pairs)
            elif path.suffix == ".md":
                errors.extend(f"{name}: {error}" for error in local_link_errors(path, content))
        except (OSError, UnicodeError, ValueError, TypeError, yaml.YAMLError) as error:
            errors.append(f"{name}: {error}")
    if errors:
        print("Repository checks failed:\n" + "\n".join(errors), file=sys.stderr)
        return 1
    print(f"Repository checks passed ({checked} tracked text files).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
