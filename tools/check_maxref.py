#!/usr/bin/env python3
"""Check that generated Max reference pages (*.maxref.xml) are well-formed XML.

Min generates these from the description strings in the externals when they
are first loaded; it does not escape '<' and '>', so the unit tests generate
the pages into tests/ and CI runs this script on them.
"""
import sys
import xml.etree.ElementTree as ET


def main(paths):
    if not paths:
        print("usage: check_maxref.py FILE.maxref.xml ...")
        return 2
    failed = 0
    for path in paths:
        try:
            root = ET.parse(path).getroot()
            if root.tag != "c74object":
                raise ValueError("root element is <%s>, expected <c74object>" % root.tag)
            print("ok   %s" % path)
        except Exception as e:  # noqa: BLE001
            print("FAIL %s: %s" % (path, e))
            failed += 1
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
