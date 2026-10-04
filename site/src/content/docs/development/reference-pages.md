---
title: Reference pages
description: The maxref pages in docs/, generated from the description strings in the source by the unit tests.
---

The reference pages in `docs/` are generated from the description strings in
the source by the unit tests (`EARMAX_TEST_GENERATE_MAXREF` in
`source/projects/shared/ear_max_test.h`, generator in `ear_max_doc.h`) and
shipped with the package. Unlike the pages Min writes when an external is
first loaded, they list every settable attribute under *Messages* as well,
since an attribute is set by sending its name as a message (`azimuth 30`),
and they escape XML. The externals set `documentation_flags::do_not_generate`
so Max does not overwrite them. After changing a description, run the tests
and commit the regenerated page; CI fails when `docs/` is out of date or a
page is not well-formed XML.
