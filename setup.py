"""setuptools based setup script for Biopython.

This uses setuptools which is now the standard python mechanism for
installing packages. If you have downloaded and uncompressed the
Biopython source code, or fetched it from git, for the simplest
installation just type the command::

    python setup.py install

However, you would normally install the latest Biopython release from
the PyPI archive with::

    pip install biopython

For more in-depth instructions, see the installation section of the
Biopython manual, linked to from:

http://biopython.org/wiki/Documentation

Or, if all else fails, feel free to write to the sign up to the Biopython
mailing list and ask for help.  See:

http://biopython.org/wiki/Mailing_lists
"""

import sys
import os

try:
    from setuptools import setup
    from setuptools import Command
    from setuptools import Extension
except ImportError:
    sys.exit(
        "We need the Python library setuptools to be installed. "
        "Try runnning: python -m ensurepip"
    )

if "bdist_wheel" in sys.argv:
    try:
        import wheel  # noqa: F401
    except ImportError:
        sys.exit(
            "We need both setuptools AND wheel packages installed "
            "for bdist_wheel to work. Try running: pip install wheel"
        )


# Make sure we have the right Python version.
MIN_PY_VER = (3, 6)
if sys.version_info[:2] < MIN_PY_VER:
    sys.stderr.write(
        ("Biopython requires Python %i.%i or later. " % MIN_PY_VER)
        + ("Python %d.%d detected.\n" % sys.version_info[:2])
    )
    sys.exit(1)


class test_biopython(Command):
    """Run all of the tests for the package.

    This is a automatic test run class to make distutils kind of act like
    perl. With this you can do:

    python setup.py build
    python setup.py install
    python setup.py test

    """

    description = "Automatically run the test suite for Biopython."
    user_options = [("offline", None, "Don't run online tests")]

    def initialize_options(self):
        """No-op, initialise options."""
        self.offline = None

    def finalize_options(self):
        """No-op, finalise options."""
        pass

    def run(self):
        """Run the tests."""
        this_dir = os.getcwd()

        # change to the test dir and run the tests
        os.chdir("Tests")
        sys.path.insert(0, "")
        import run_tests

        if self.offline:
            run_tests.main(["--offline"])
        else:
            run_tests.main([])

        # change back to the current directory
        os.chdir(this_dir)


def can_import(module_name):
    """Check we can import the requested module."""
    try:
        return __import__(module_name)
    except ImportError:
        return None


# Using requirements.txt is preferred for an application
# (and likely will pin specific version numbers), using
# setup.py's install_requires is preferred for a library
# (and should try not to be overly narrow with versions).
REQUIRES = ["numpy"]

# --- set up the packages we are going to install
# standard biopython packages
PACKAGES = [
    "biopython",
    "biopython.Affy",
    "biopython.Align",
    "biopython.Align.Applications",
    "biopython.Align.substitution_matrices",
    "biopython.AlignIO",
    "biopython.Alphabet",
    "biopython.Application",
    "biopython.BioSQL",
    "biopython.Blast",
    "biopython.CAPS",
    "biopython.Cluster",
    "biopython.codonalign",
    "biopython.Compass",
    "biopython.Crystal",
    "biopython.Data",
    "biopython.Emboss",
    "biopython.Entrez",
    "biopython.ExPASy",
    "biopython.FSSP",
    "biopython.GenBank",
    "biopython.Geo",
    "biopython.Graphics",
    "biopython.Graphics.GenomeDiagram",
    "biopython.HMM",
    "biopython.KEGG",
    "biopython.KEGG.Compound",
    "biopython.KEGG.Enzyme",
    "biopython.KEGG.Gene",
    "biopython.KEGG.Map",
    "biopython.PDB.mmtf",
    "biopython.KEGG.KGML",
    "biopython.Medline",
    "biopython.motifs",
    "biopython.motifs.applications",
    "biopython.motifs.jaspar",
    "biopython.Nexus",
    "biopython.NMR",
    "biopython.Pathway",
    "biopython.Pathway.Rep",
    "biopython.PDB",
    "biopython.phenotype",
    "biopython.PopGen",
    "biopython.PopGen.GenePop",
    "biopython.Restriction",
    "biopython.SCOP",
    "biopython.SearchIO",
    "biopython.SearchIO._legacy",
    "biopython.SearchIO._model",
    "biopython.SearchIO.BlastIO",
    "biopython.SearchIO.HHsuiteIO",
    "biopython.SearchIO.HmmerIO",
    "biopython.SearchIO.ExonerateIO",
    "biopython.SearchIO.InterproscanIO",
    "biopython.SeqIO",
    "biopython.SeqUtils",
    "biopython.Sequencing",
    "biopython.Sequencing.Applications",
    "biopython.Statistics",
    "biopython.SubsMat",
    "biopython.SVDSuperimposer",
    "biopython.PDB.QCPSuperimposer",
    "biopython.SwissProt",
    "biopython.TogoWS",
    "biopython.Phylo",
    "biopython.Phylo.Applications",
    "biopython.Phylo.PAML",
    "biopython.UniGene",
    "biopython.UniProt",
    "biopython.Wise",
]

EXTENSIONS = [
    Extension("biopython._seqobject", ["biopython/_seqobject.c"]),
    Extension("biopython.Align._aligners", ["biopython/Align/_aligners.c"]),
    Extension("biopython.cpairwise2", ["biopython/cpairwise2module.c"]),
    Extension("biopython.Nexus.cnexus", ["biopython/Nexus/cnexus.c"]),
    Extension(
        "biopython.PDB.QCPSuperimposer.qcprotmodule",
        ["biopython/PDB/QCPSuperimposer/qcprotmodule.c"],
    ),
    Extension("biopython.motifs._pwm", ["biopython/motifs/_pwm.c"]),
    Extension(
        "biopython.Cluster._cluster", ["biopython/Cluster/cluster.c", "biopython/Cluster/clustermodule.c"]
    ),
    Extension("biopython.PDB.kdtrees", ["biopython/PDB/kdtrees.c"]),
]

# We now define the Biopython version number in biopython/__init__.py
# Here we can't use "import biopython" then "biopython.__version__" as that
# would tell us the version of Biopython already installed (if any).
__version__ = "Undefined"
for line in open("biopython/__init__.py"):
    if line.startswith("__version__"):
        exec(line.strip())

# We now load in our reStructuredText README.rst file to pass explicitly in the
# metadata, since at time of writing PyPI did not do this for us.
#
# Must make encoding explicit to avoid any conflict with the local default.
# Currently keeping README as ASCII (might switch to UTF8 later if needed).
# If any invalid character does appear in README, this will fail and alert us.
with open("README.rst", encoding="ascii") as handle:
    readme_rst = handle.read()

setup(
    name="biopython",
    version=__version__,
    author="The Biopython Contributors",
    author_email="biopython@biopython.org",
    url="https://biopython.org/",
    description="Freely available tools for computational molecular biology.",
    long_description=readme_rst,
    project_urls={
        "Documentation": "https://biopython.org/wiki/Documentation",
        "Source": "https://github.com/biopython/biopython/",
        "Tracker": "https://github.com/biopython/biopython/issues",
    },
    classifiers=[
        "Development Status :: 5 - Production/Stable",
        "Intended Audience :: Developers",
        "Intended Audience :: Science/Research",
        "License :: Freely Distributable",
        # Technically the "Biopython License Agreement" is not OSI approved,
        # but is almost https://opensource.org/licenses/HPND so might put:
        # 'License :: OSI Approved',
        # To resolve this we are moving to dual-licensing with 3-clause BSD:
        # 'License :: OSI Approved :: BSD License',
        "Operating System :: OS Independent",
        "Programming Language :: Python",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.6",
        "Programming Language :: Python :: 3.7",
        "Programming Language :: Python :: 3.8",
        "Topic :: Scientific/Engineering",
        "Topic :: Scientific/Engineering :: Bio-Informatics",
        "Topic :: Software Development :: Libraries :: Python Modules",
    ],
    cmdclass={"test": test_biopython},
    packages=PACKAGES,
    ext_modules=EXTENSIONS,
    include_package_data=True,  # done via MANIFEST.in under setuptools
    install_requires=REQUIRES,
    python_requires=">=%i.%i" % MIN_PY_VER,
)
