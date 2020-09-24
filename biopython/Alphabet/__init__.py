# Copyright 2000-2002 by Andrew Dalke.
# Revisions copyright 2007-2010 by Peter Cock.
# All rights reserved.
#
# This file is part of the Biopython distribution and governed by your
# choice of the "Biopython License Agreement" or the "BSD 3-Clause License".
# Please see the LICENSE file that should have been included as part of this
# package.
"""Alphabets were previously used to declare sequence type and letters (OBSOLETE).

The design of biopython.Aphabet included a number of historic design choices
which, with the benefit of hindsight, were regretable. biopython.Alphabet was
therefore removed from Biopython in release 1.78. Instead, the molecule type is
included as an annotation on SeqRecords where appropriate.

Please see
https://github.com/biopython/biopython/issues/3156
for examples showing how to transition from biopython.Alphabet to molecule type
annotations.
"""

raise ImportError(
    "biopython.Alphabet has been removed from biopythonpython. In many cases, the alphabet can simply be ignored and removed from scripts. In a few cases, you may need to specify the ``molecule_type`` as an annotation on a SeqRecord for your script to work correctly. Please see https://github.com/biopython/biopython/issues/3156 for more information."
)
