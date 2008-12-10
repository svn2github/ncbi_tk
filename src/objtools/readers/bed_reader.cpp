/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author:  Frank Ludwig
 *
 * File Description:
 *   BED file reader
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/stream_utils.hpp>

#include <util/static_map.hpp>

#include <serial/iterator.hpp>
#include <serial/objistrasn.hpp>

// Objects includes
#include <objects/general/Int_fuzz.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Dbtag.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/Gene_ref.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Feat_id.hpp>

#include <objtools/readers/reader_exception.hpp>
#include <objtools/readers/bed_reader.hpp>
#include <objtools/error_codes.hpp>

#include <algorithm>


#define NCBI_USE_ERRCODE_X   Objtools_Rd_RepMask

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
CBedReader::CBedReader(
    int flags )
//  ----------------------------------------------------------------------------
    : m_columncount( 0 )
    , m_usescore( false )
{
}


//  ----------------------------------------------------------------------------
CBedReader::~CBedReader()
//  ----------------------------------------------------------------------------
{
}

//  ----------------------------------------------------------------------------
bool CBedReader::VerifyFormat(
    CNcbiIstream& is )
//  ----------------------------------------------------------------------------
{
    string line;
    int linecount = 0;
    size_t columncount = 0;
    bool verify = true;

    while ( ! is.eof() && linecount < 5 ) {
        NcbiGetlineEOL( is, line );
        if ( NStr::TruncateSpaces( line ).empty() ) {
            continue;
        }
        if ( x_IsMetaInformation( line ) ) {
            continue;
        }        
        ++linecount;

        //
        // For now: at least three, at most 12 columns. All lines have same
        //  number of columns.
        // Todo: Apply more stringent criteria, based on syntactic content of
        //  columns.
        //
        vector<string> columns;
        NStr::Tokenize( line, " \t", columns, NStr::eMergeDelims );
        if (columns.size() < 3 || columns.size() > 12) {
            verify = false;
            break;
        }
        if ( columns.size() != columncount ) {
            if ( columncount == 0 ) {
                columncount = columns.size();
            }
            else {
                verify = false;
                break;
            }
        }
    }
    is.clear();  // in case we reached eof
    is.seekg( 0 );
    return verify;
}

//  ----------------------------------------------------------------------------
void CBedReader::Read( 
    CNcbiIstream& input, 
    CRef<CSeq_annot>& annot )
//  ----------------------------------------------------------------------------
{
    string line;
    int linecount = 0;

    while ( ! input.eof() ) {
        ++linecount;
        NcbiGetlineEOL( input, line );
        if ( NStr::TruncateSpaces( line ).empty() ) {
            continue;
        }
        if ( x_IsMetaInformation( line ) ) {
            if ( ! x_ProcessMetaInformation( line ) ) {
                cerr << "Warning: Junk meta info encountered in line "
                     << linecount << endl;
            }
            continue;
        }
        if ( ! x_ParseFeature( line, annot ) ) {
            cerr << "Warning: Junk record encountered in line " << linecount
                 << endl;
            continue;
        }
    }
}

//  ----------------------------------------------------------------------------
bool CBedReader::x_IsMetaInformation(
    const string& line )
//  ----------------------------------------------------------------------------
{
    if ( NStr::StartsWith( line, "track" ) ) {
        return true;
    }
    if ( NStr::StartsWith( line, "browser" ) ) {
        return true;
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool CBedReader::x_ProcessMetaInformation(
    const string& line )
//  ----------------------------------------------------------------------------
{
    vector<string> fields;
    NStr::Tokenize( line, " \t", fields, NStr::eMergeDelims );
    if ( fields[0] == "track" ) {
        for ( vector<string>::size_type i=1; i < fields.size(); ++i ) {
            if ( NStr::StartsWith( fields[i], "useScore=" ) ) {
                vector<string> splits;
                NStr::Tokenize( fields[i], "=", splits, NStr::eMergeDelims );
                if ( splits.size() == 2 ) {
                    m_usescore = (1 == NStr::StringToInt(splits[1]));
                }
                else {
                    return false;
                }
            }
        }
        return true;
    }
    if ( fields[0] == "browser" ) {
        return true;
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool CBedReader::x_ParseFeature(
    const string& record,
    CRef<CSeq_annot>& annot )
//  ----------------------------------------------------------------------------
{
    CSeq_annot::C_Data::TFtable& ftable = annot->SetData().SetFtable();
    CRef<CSeq_feat> feature;
    vector<string> fields;

    //  parse
    NStr::Tokenize( record, " \t", fields, NStr::eMergeDelims );
    if (fields.size() != m_columncount) {
        if ( 0 == m_columncount ) {
            m_columncount = fields.size();
        }
        else {
            return false;
        }
    }

    //  assign
    feature.Reset( new CSeq_feat );
    try {
        x_SetFeatureLocation( feature, fields );
        x_SetFeatureDisplayData( feature, fields );
    }
    catch (...) {
        return false;
    }
    ftable.push_back( feature );
    return true;
}

//  ----------------------------------------------------------------------------
void CBedReader::x_SetFeatureDisplayData(
    CRef<CSeq_feat>& feature,
    const vector<string>& fields )
//  ----------------------------------------------------------------------------
{
    CRef<CUser_object> display_data( new CUser_object );
    display_data->SetType().SetStr( "Display Data" );
    if ( m_columncount >= 4 ) {
        display_data->AddField( "name", fields[3] );
    }
    if ( m_columncount >= 5 ) {
        if ( !m_usescore ) {
            display_data->AddField( "score", NStr::StringToInt(fields[4]) );
        }
        else {
            display_data->AddField( "greylevel", NStr::StringToInt(fields[4]) );
        }
    }
    if ( m_columncount >= 6 ) {
        display_data->AddField( "thickStart", NStr::StringToInt(fields[6]) );
    }
    if ( m_columncount >= 7 ) {
        display_data->AddField( "thickEnd", NStr::StringToInt(fields[7]) - 1 );
    }
    if ( m_columncount >= 8 ) {
        display_data->AddField( "itemRGB", NStr::StringToInt(fields[8]) );
    }
    if ( m_columncount >= 9 ) {
        display_data->AddField( "blockCount", NStr::StringToInt(fields[9]) );
    }
    if ( m_columncount >= 10 ) {
        display_data->AddField( "blockSizes", fields[10] );
    }
    if ( m_columncount >= 11 ) {
        display_data->AddField( "blockStarts", fields[11] );
    }
    feature->SetData().SetUser( *display_data );
}

//  ----------------------------------------------------------------------------
void CBedReader::x_SetFeatureLocation(
    CRef<CSeq_feat>& feature,
    const vector<string>& fields )
//  ----------------------------------------------------------------------------
{
    feature->ResetLocation();
    
    CRef<CSeq_id> id( new CSeq_id() );
    id->SetLocal().SetStr( fields[0] );

    CRef<CSeq_loc> location( new CSeq_loc );
    CSeq_interval& interval = location->SetInt();
    interval.SetFrom( NStr::StringToInt( fields[1] ) );
    interval.SetTo( NStr::StringToInt( fields[2] ) - 1 );
    interval.SetStrand( 
        ( fields[5] == "+" ) ? eNa_strand_plus : eNa_strand_minus );
    location->SetId( *id );
    
    feature->SetLocation( *location );
}

END_objects_SCOPE
END_NCBI_SCOPE
