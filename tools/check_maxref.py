#!/usr/bin/env python3
"""Check that generated Max reference pages (*.maxref.xml) are well-formed XML.

The unit tests generate them into docs/ from the description strings in the
externals (source/projects/shared/ear_max_doc.h); CI runs this script on the
committed pages.
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
