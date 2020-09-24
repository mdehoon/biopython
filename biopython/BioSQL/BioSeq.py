# Copyright 2002 by Andrew Dalke.  All rights reserved.
# Revisions 2007-2016 copyright by Peter Cock.  All rights reserved.
# Revisions 2008-2009 copyright by Cymon J. Cox.  All rights reserved.
#
# This file is part of the Biopython distribution and governed by your
# choice of the "Biopython License Agreement" or the "BSD 3-Clause License".
# Please see the LICENSE file that should have been included as part of this
# package.
#
# Note that BioSQL (including the database schema and scripts) is
# available and licensed separately.  Please consult www.biosql.org
"""Implementations of Biopython-like Seq objects on top of BioSQL.

This allows retrieval of items stored in a BioSQL database using
a biopython-like Seq interface.

Note: Currently we do not support recording per-letter-annotations
(like quality scores) in BioSQL.
"""
import sys

from biopython.Seq import Seq
from biopython import SeqFeature


class DBSeq(Seq):
    """BioSQL equivalent of the Biopython Seq object."""

    def __new__(cls, adaptor, primary_id, start=0, length=None):
        return super().__new__(cls, None)

    def __init__(self, adaptor, primary_id, start=0, length=None):
        """Create a new DBSeq object referring to a BioSQL entry.

        You wouldn't normally create a DBSeq object yourself, this is done
        for you when retrieving a DBSeqRecord object from the database.
        """
        self._primary_id = primary_id
        self._adaptor = adaptor
        self.start = start
        if length is not None:
            self._length = length
        (
            self._biodatabase_id,
            self._taxon_id,
            self.name,
            accession,
            version,
            self._identifier,
            self._division,
            self.description,
        ) = self._adaptor.execute_one(
            "SELECT biodatabase_id, taxon_id, name, accession, version,"
            " identifier, division, description"
            " FROM bioentry"
            " WHERE bioentry_id = %s",
            (self._primary_id,),
        )
        if version and version != "0":
            self.id = "%s.%s" % (accession, version)
        else:
            self.id = accession
        self._defined = None

    def __len__(self):
        """Return the length of the sequence."""
        if not hasattr(self, "_length"):
            adaptor = self._adaptor
            primary_id = self._primary_id
            # The database schema ensures there will be only one matching row
            seqs = adaptor.execute_and_fetchall(
                "SELECT length FROM biosequence WHERE bioentry_id = %s", (primary_id,)
            )
            if seqs:
                assert len(seqs) == 1
                (given_length,) = seqs[0]
                self._length = int(given_length)
            else:
                self._length = None
        return self._length

    def __getitem__(self, index):  # Seq API requirement
        """Return a subsequence or single letter."""
        length = len(self)
        if isinstance(index, int):
            # Return a single letter as a string
            i = index
            if i < 0:
                if -i > length:
                    raise IndexError(i)
                i = i + length
            elif i >= length:
                raise IndexError(i)
            seq, self._defined = self._adaptor.get_subseq_as_string(
                self._primary_id, self.start + i, self.start + i + 1
            )
            return seq
        if not isinstance(index, slice):
            raise TypeError("Unexpected index type")

        # Return the (sub)sequence as another DBSeq or Seq object
        # (see the Seq obect's __getitem__ method)
        if index.start is None:
            i = 0
        else:
            i = index.start
        if i < 0:
            # Map to equivalent positive index
            if -i > length:
                raise IndexError(i)
            i += length
        elif i >= length:
            # Trivial case, should return empty string!
            i = length

        if index.stop is None:
            j = length
        else:
            j = index.stop
        if j < 0:
            # Map to equivalent positive index
            if -j > length:
                raise IndexError(j)
            j += length
        elif j >= length:
            j = length

        if i >= j:
            # Trivial case, empty string.
            return Seq("")
        elif index.step is None or index.step == 1:
            # Easy case - can return a DBSeq with the start and end adjusted
            return self.__class__(self._adaptor, self._primary_id, self.start + i, j - i)
        else:
            # Tricky.  Will have to create a Seq object because of the stride
            full, self._defined = self._adaptor.get_subseq_as_string(
                self._primary_id, self.start + i, self.start + j
            )
            return Seq(full[:: index.step])

    def __str__(self):
        """Return the full sequence as a python string."""
        length = len(self)
        sequence, self._defined = self._adaptor.get_subseq_as_string(
            self._primary_id, self.start, self.start + length
        )
        return sequence

    @property
    def annotations(self):
        if not hasattr(self, "_annotations"):
            self._annotations = self._retrieve_annotations()
            if self._identifier:
                self._annotations["gi"] = self._identifier
            if self._division:
                self._annotations["data_file_division"] = self._division
        return self._annotations

    @property
    def features(self):
        if not hasattr(self, "_features"):
            length = len(self)
            start = self.start
            stop = start + length
            self._features = []
            for f in self._retrieve_features():
                if f.ref or f.ref_db:
                    self._features.append(f)
                else:
                    try:
                        feature_start = int(f.location.start)
                    except TypeError:  # e.g. UnknownPosition
                        continue
                    try:
                        feature_end = int(f.location.end)
                    except TypeError:  # e.g. UnknownPosition
                        continue
                    if start <= feature_start and feature_end <= stop:
                        self._features.append(f._shift(-start))
        return self._features

    @property
    def dbxrefs(self):
        if not hasattr(self, "_dbxrefs"):
            self._dbxrefs = self._retrieve_dbxrefs()
        return self._dbxrefs

    @property
    def defined(self):
        if self._defined is None:
            seq = self._adaptor.execute_one("""select SUBSTR(seq, 1, 3)
                  from biosequence where bioentry_id = %s""",
                  (self._primary_id,))
            if seq is None:
                # UnknownSeq
                self._defined = False
            else:
                self._defined = True
        return self._defined

    def __add__(self, other):
        """Add another sequence or string to this sequence.

        The sequence is first converted to a Seq object before the addition.
        The returned object is a Seq object, not a DBSeq object.
        """
        return Seq(self) + other

    def __radd__(self, other):
        """Add another sequence or string to the left.

        The sequence is first converted to a Seq object before the addition.
        The returned object is a Seq object, not a DBSeq object.
        """
        return other + Seq(self)

    def __mul__(self, other):
        """Multiply sequence by an integer.

        The sequence is first converted to a Seq object before multiplication.
        The returned object is a Seq object, not a DBSeq object.
        """
        return Seq(self) * other

    def __rmul__(self, other):
        """Multiply integer by a sequence.

        The sequence is first converted to a Seq object before multiplication.
        The returned object is a Seq object, not a DBSeq object.
        """
        return other * Seq(self)

    def __imul__(self, other):
        """Multiply sequence by integer in-place.

        The sequence is first converted to a Seq object before multiplication.
        The returned object is a Seq object, not a DBSeq object.
        """
        return Seq(self) * other

    def _retrieve_alphabet(self):
        adaptor = self._adaptor
        primary_id = self._primary_id
        results = adaptor.execute_and_fetchall(
            "SELECT alphabet FROM biosequence WHERE bioentry_id = %s", (primary_id,),
        )
        assert len(results) == 1
        alphabets = results[0]
        assert len(alphabets) == 1
        alphabet = alphabets[0]
        if alphabet == "dna":
            molecule_type = "DNA"
        elif alphabet == "rna":
            molecule_type = "RNA"
        elif alphabet == "protein":
            molecule_type = "protein"
        else:
            molecule_type = None
        if molecule_type is not None:
            return {"molecule_type": molecule_type}
        else:
            return {}

    def _retrieve_qualifier_value(self):
        adaptor = self._adaptor
        primary_id = self._primary_id
        qvs = adaptor.execute_and_fetchall(
            "SELECT name, value"
            " FROM bioentry_qualifier_value JOIN term USING (term_id)"
            " WHERE bioentry_id = %s"
            ' ORDER BY "rank"',
            (primary_id,),
        )
        qualifiers = {}
        for name, value in qvs:
            if name == "keyword":
                name = "keywords"
            # See handling of "date" in Loader.py
            elif name == "date_changed":
                name = "date"
            elif name == "secondary_accession":
                name = "accessions"
            qualifiers.setdefault(name, []).append(value)
        return qualifiers

    def _retrieve_reference(self):
        adaptor = self._adaptor
        primary_id = self._primary_id
        # XXX dbxref_qualifier_value

        refs = adaptor.execute_and_fetchall(
            "SELECT start_pos, end_pos, "
            " location, title, authors,"
            " dbname, accession"
            " FROM bioentry_reference"
            " JOIN reference USING (reference_id)"
            " LEFT JOIN dbxref USING (dbxref_id)"
            " WHERE bioentry_id = %s"
            ' ORDER BY "rank"',
            (primary_id,),
        )
        references = []
        for start, end, location, title, authors, dbname, accession in refs:
            reference = SeqFeature.Reference()
            # If the start/end are missing, reference.location is an empty list
            if (start is not None) or (end is not None):
                if start is not None:
                    start -= 1  # python counting
                reference.location = [SeqFeature.FeatureLocation(start, end)]
            # Don't replace the default "" with None.
            if authors:
                reference.authors = authors
            if title:
                reference.title = title
            reference.journal = location
            if dbname == "PUBMED":
                reference.pubmed_id = accession
            elif dbname == "MEDLINE":
                reference.medline_id = accession
            references.append(reference)
        if references:
            return {"references": references}
        else:
            return {}

    def _retrieve_taxon(self):
        adaptor = self._adaptor
        primary_id = self._primary_id
        taxon_id = self._taxon_id
        a = {}
        common_names = adaptor.execute_and_fetch_col0(
            "SELECT name FROM taxon_name WHERE taxon_id = %s"
            " AND name_class = 'genbank common name'",
            (taxon_id,),
        )
        if common_names:
            a["source"] = common_names[0]
        scientific_names = adaptor.execute_and_fetch_col0(
            "SELECT name FROM taxon_name WHERE taxon_id = %s"
            " AND name_class = 'scientific name'",
            (taxon_id,),
        )
        if scientific_names:
            a["organism"] = scientific_names[0]
        ncbi_taxids = adaptor.execute_and_fetch_col0(
            "SELECT ncbi_taxon_id FROM taxon WHERE taxon_id = %s", (taxon_id,)
        )
        if ncbi_taxids and ncbi_taxids[0] and ncbi_taxids[0] != "0":
            a["ncbi_taxid"] = ncbi_taxids[0]

        # Old code used the left/right values in the taxon table to get the
        # taxonomy lineage in one SQL command.  This was actually very slow,
        # and would fail if the (optional) left/right values were missing.
        #
        # The following code is based on a contribution from Eric Gibert, and
        # relies on the taxon table's parent_taxon_id field only (ignoring the
        # optional left/right values).  This means that it has to make a
        # separate SQL query for each entry in the lineage, but it does still
        # appear to be *much* faster.  See Bug 2494.
        taxonomy = []
        while taxon_id:
            name, rank, parent_taxon_id = adaptor.execute_one(
                "SELECT taxon_name.name, taxon.node_rank, taxon.parent_taxon_id"
                " FROM taxon, taxon_name"
                " WHERE taxon.taxon_id=taxon_name.taxon_id"
                " AND taxon_name.name_class='scientific name'"
                " AND taxon.taxon_id = %s",
                (taxon_id,),
            )
            if taxon_id == parent_taxon_id:
                # If the taxon table has been populated by the BioSQL script
                # load_ncbi_taxonomy.pl this is how top parent nodes are stored.
                # Personally, I would have used a NULL parent_taxon_id here.
                break

            taxonomy.insert(0, name)
            taxon_id = parent_taxon_id

        if taxonomy:
            a["taxonomy"] = taxonomy
        return a

    def _retrieve_comment(self):
        adaptor = self._adaptor
        primary_id = self._primary_id
        qvs = adaptor.execute_and_fetchall(
            'SELECT comment_text FROM comment WHERE bioentry_id=%s ORDER BY "rank"',
            (primary_id,),
        )
        comments = [comm[0] for comm in qvs]
        # Don't want to add an empty list...
        if comments:
            return {"comment": comments}
        else:
            return {}

    def _retrieve_annotations(self):
        annotations = {}
        annotations.update(self._retrieve_alphabet())
        annotations.update(self._retrieve_qualifier_value())
        annotations.update(self._retrieve_reference())
        annotations.update(self._retrieve_taxon())
        annotations.update(self._retrieve_comment())
        return annotations

    def _retrieve_location_qualifier_value(self, location_id):
        adaptor = self._adaptor
        value = adaptor.execute_and_fetch_col0(
            "SELECT value FROM location_qualifier_value WHERE location_id = %s",
            (location_id,),
        )
        try:
            return value[0]
        except IndexError:
            return ""

    def _retrieve_features(self):
        adaptor = self._adaptor
        primary_id = self._primary_id
        sql = (
            'SELECT seqfeature_id, type.name, "rank"'
            " FROM seqfeature join term type on (type_term_id = type.term_id)"
            " WHERE bioentry_id = %s"
            ' ORDER BY "rank"'
        )
        results = adaptor.execute_and_fetchall(sql, (primary_id,))
        seq_feature_list = []
        for seqfeature_id, seqfeature_type, seqfeature_rank in results:
            # Get qualifiers [except for db_xref which is stored separately]
            qvs = adaptor.execute_and_fetchall(
                "SELECT name, value"
                " FROM seqfeature_qualifier_value  join term using (term_id)"
                " WHERE seqfeature_id = %s"
                ' ORDER BY "rank"',
                (seqfeature_id,),
            )
            qualifiers = {}
            for qv_name, qv_value in qvs:
                qualifiers.setdefault(qv_name, []).append(qv_value)
            # Get db_xrefs [special case of qualifiers]
            qvs = adaptor.execute_and_fetchall(
                "SELECT dbxref.dbname, dbxref.accession"
                " FROM dbxref join seqfeature_dbxref using (dbxref_id)"
                " WHERE seqfeature_dbxref.seqfeature_id = %s"
                ' ORDER BY "rank"',
                (seqfeature_id,),
            )
            for qv_name, qv_value in qvs:
                value = "%s:%s" % (qv_name, qv_value)
                qualifiers.setdefault("db_xref", []).append(value)
            # Get locations
            results = adaptor.execute_and_fetchall(
                "SELECT location_id, start_pos, end_pos, strand"
                " FROM location"
                " WHERE seqfeature_id = %s"
                ' ORDER BY "rank"',
                (seqfeature_id,),
            )
            locations = []
            # convert to Python standard form
            # Convert strand = 0 to strand = None
            # re: comment in Loader.py:
            # Biopython uses None when we don't know strand information but
            # BioSQL requires something (non null) and sets this as zero
            # So we'll use the strand or 0 if Biopython spits out None
            for location_id, start, end, strand in results:
                if start:
                    start -= 1
                if strand == 0:
                    strand = None
                if strand not in (+1, -1, None):
                    raise ValueError(
                        "Invalid strand %s found in database for "
                        "seqfeature_id %s" % (strand, seqfeature_id)
                    )
                if start is not None and end is not None and end < start:
                    import warnings
                    from biopython import BiopythonWarning

                    warnings.warn(
                        "Inverted location start/end (%i and %i) for "
                        "seqfeature_id %s" % (start, end, seqfeature_id),
                        BiopythonWarning,
                    )

                # For SwissProt unknown positions (?)
                if start is None:
                    start = SeqFeature.UnknownPosition()
                if end is None:
                    end = SeqFeature.UnknownPosition()

                locations.append((location_id, start, end, strand))
            # Get possible remote reference information
            remote_results = adaptor.execute_and_fetchall(
                "SELECT location_id, dbname, accession, version"
                " FROM location join dbxref using (dbxref_id)"
                " WHERE seqfeature_id = %s",
                (seqfeature_id,),
            )
            lookup = {}
            for location_id, dbname, accession, version in remote_results:
                if version and version != "0":
                    v = "%s.%s" % (accession, version)
                else:
                    v = accession
                # subfeature remote location db_ref are stored as a empty string
                # when not present
                if dbname == "":
                    dbname = None
                lookup[location_id] = (dbname, v)

            feature = SeqFeature.SeqFeature(type=seqfeature_type)
            # Store the key as a private property
            feature._seqfeature_id = seqfeature_id
            feature.qualifiers = qualifiers
            if len(locations) == 0:
                pass
            elif len(locations) == 1:
                location_id, start, end, strand = locations[0]
                # See Bug 2677, we currently don't record the location_operator
                # For consistency with older versions Biopython, default to "".
                feature.location_operator = self._retrieve_location_qualifier_value(location_id)
                dbname, version = lookup.get(location_id, (None, None))
                feature.location = SeqFeature.FeatureLocation(start, end)
                feature.strand = strand
                feature.ref_db = dbname
                feature.ref = version
            else:
                locs = []
                for location in locations:
                    location_id, start, end, strand = location
                    dbname, version = lookup.get(location_id, (None, None))
                    locs.append(
                        SeqFeature.FeatureLocation(
                            start, end, strand=strand, ref=version, ref_db=dbname
                        )
                    )
                # Locations are typically in biological in order (see negative
                # strands below), but because of remote locations for
                # sub-features they are not necessarily in numerical order:
                strands = {l.strand for l in locs}
                if len(strands) == 1 and -1 in strands:
                    # Evil hack time for backwards compatibility
                    # TODO - Check if BioPerl and (old) Biopython did the same,
                    # we may have an existing incompatibility lurking here...
                    locs = locs[::-1]
                feature.location = SeqFeature.CompoundLocation(locs, "join")
                # TODO - See Bug 2677 - we don't yet record location operator,
                # so for consistency with older versions of Biopython default
                # to assuming its a join.
            seq_feature_list.append(feature)
        return seq_feature_list

    def _retrieve_dbxrefs(self):
        """Retrieve the database cross references for the sequence (PRIVATE)."""
        adaptor = self._adaptor
        primary_id = self._primary_id
        _dbxrefs = []
        dbxrefs = adaptor.execute_and_fetchall(
            "SELECT dbname, accession, version"
            " FROM bioentry_dbxref join dbxref using (dbxref_id)"
            " WHERE bioentry_id = %s"
            ' ORDER BY "rank"',
            (primary_id,),
        )
        for dbname, accession, version in dbxrefs:
            if version and version != "0":
                v = "%s.%s" % (accession, version)
            else:
                v = accession
            _dbxrefs.append("%s:%s" % (dbname, v))
        return _dbxrefs
