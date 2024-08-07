# Copyright 2012 by Christian Brueffer.  All rights reserved.
#
# This code is part of the Biopython distribution and governed by its
# license.  Please see the LICENSE file that should have been included
# as part of this package.

"""Tests for XXmotif tool."""

import glob
import os
import shutil
import sys
import unittest
import warnings

from Bio import BiopythonDeprecationWarning
from Bio import MissingExternalDependencyError
from Bio import SeqIO

with warnings.catch_warnings():
    warnings.simplefilter("ignore", category=BiopythonDeprecationWarning)
    from Bio.Application import ApplicationError
    from Bio.motifs.applications import XXmotifCommandline


# Try to avoid problems when the OS is in another language
os.environ["LANG"] = "C"

xxmotif_exe = None
if sys.platform == "win32":
    # TODO
    raise MissingExternalDependencyError(
        "Testing this on Windows is not implemented yet"
    )
else:
    from subprocess import getoutput

    output = getoutput("XXmotif")
    if output.find("== XXmotif version") != -1:
        xxmotif_exe = "XXmotif"

if not xxmotif_exe:
    raise MissingExternalDependencyError(
        "Install XXmotif if you want to use XXmotif from Biopython."
    )


class XXmotifTestCase(unittest.TestCase):
    def setUp(self):
        self.out_dir = "xxmotif-temp"
        self.files_to_clean = set()

    def tearDown(self):
        for filename in self.files_to_clean:
            if os.path.isfile(filename):
                os.remove(filename)

        if os.path.isdir(self.out_dir):
            shutil.rmtree(self.out_dir)

    def standard_test_procedure(self, cline):
        """Shared test procedure used by all tests."""
        output, error = cline()

        self.assertTrue(os.path.isdir(self.out_dir))
        self.assertTrue(glob.glob(os.path.join(self.out_dir, "*.meme")))
        self.assertTrue(glob.glob(os.path.join(self.out_dir, "*_MotifFile.txt")))
        self.assertTrue(glob.glob(os.path.join(self.out_dir, "*_Pvals.txt")))
        self.assertTrue(glob.glob(os.path.join(self.out_dir, "*.pwm")))
        self.assertTrue(glob.glob(os.path.join(self.out_dir, "*_sequence.txt")))

        # TODO
        # Parsing the MEME file would be nice, but unfortunately the
        # MEME parser does not like what XXmotif produces yet.

    def copy_and_mark_for_cleanup(self, path):
        """Copy file to working directory and marks it for removal.

        XXmotif currently only handles a canonical filename as input, no paths.
        This method copies the specified file in the specified path to the
        current working directory and marks it for removal.
        """
        filename = os.path.split(path)[1]

        shutil.copyfile(path, filename)
        self.add_file_to_clean(filename)

        return filename

    def add_file_to_clean(self, filename):
        """Add a file for deferred removal by the tearDown routine."""
        self.files_to_clean.add(filename)


class XXmotifTestErrorConditions(XXmotifTestCase):
    def test_empty_file(self):
        """Test a non-existing input file."""
        input_file = "does_not_exist.fasta"
        self.assertFalse(os.path.isfile(input_file))

        cline = XXmotifCommandline(outdir=self.out_dir, seqfile=input_file)

        try:
            stdout, stderr = cline()
        except ApplicationError as err:
            self.assertEqual(err.returncode, 255)
        else:
            self.fail(f"Should have failed, returned:\n{stdout}\n{stderr}")

    def test_invalid_format(self):
        """Test an input file in an invalid format."""
        input_file = self.copy_and_mark_for_cleanup("Medline/pubmed_result1.txt")

        cline = XXmotifCommandline(outdir=self.out_dir, seqfile=input_file)

        try:
            stdout, stderr = cline()
        except ApplicationError as err:
            self.assertEqual(err.returncode, 255)
        else:
            self.fail(f"Should have failed, returned:\n{stdout}\n{stderr}")

    def test_output_directory_with_space(self):
        """Test an output directory containing a space."""
        temp_out_dir = "xxmotif test"
        input_file = self.copy_and_mark_for_cleanup("Fasta/f002")

        try:
            XXmotifCommandline(outdir=temp_out_dir, seqfile=input_file)
        except ValueError:
            pass
        else:
            self.fail("expected ValueError")


class XXmotifTestNormalConditions(XXmotifTestCase):
    def test_fasta_one_sequence(self):
        """Test a fasta input file containing only one sequence."""
        record = list(SeqIO.parse("Registry/seqs.fasta", "fasta"))[0]
        input_file = "seq.fasta"
        with open(input_file, "w") as handle:
            SeqIO.write(record, handle, "fasta")

        cline = XXmotifCommandline(outdir=self.out_dir, seqfile=input_file)

        self.add_file_to_clean(input_file)
        self.standard_test_procedure(cline)

    def test_properties(self):
        """Test setting options via properties."""
        input_file = self.copy_and_mark_for_cleanup("Fasta/f002")

        cline = XXmotifCommandline(outdir=self.out_dir, seqfile=input_file)

        cline.revcomp = True
        cline.pseudo = 20
        cline.startmotif = "ACGGGT"

        self.standard_test_procedure(cline)

    def test_large_fasta_file(self):
        """Test a large fasta input file."""
        records = list(SeqIO.parse("NBRF/B_nuc.pir", "pir"))
        input_file = "temp_b_nuc.fasta"
        with open(input_file, "w") as handle:
            SeqIO.write(records, handle, "fasta")

        cline = XXmotifCommandline(outdir=self.out_dir, seqfile=input_file)

        self.add_file_to_clean(input_file)
        self.standard_test_procedure(cline)

    def test_input_filename_with_space(self):
        """Test an input filename containing a space."""
        records = SeqIO.parse("Phylip/hennigian.phy", "phylip")
        input_file = "temp horses.fasta"
        with open(input_file, "w") as handle:
            SeqIO.write(records, handle, "fasta")

        cline = XXmotifCommandline(outdir=self.out_dir, seqfile=input_file)

        self.add_file_to_clean(input_file)
        self.standard_test_procedure(cline)


if __name__ == "__main__":
    runner = unittest.TextTestRunner(verbosity=2)
    unittest.main(testRunner=runner)
