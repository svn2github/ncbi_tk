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
* Authors:  Paul Thiessen
*
* File Description:
*      implementation of non-GUI part of main sequence/alignment viewer
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.16  2001/03/01 20:15:29  thiessen
* major rearrangement of sequence viewer code into base and derived classes
*
* ===========================================================================
*/

#ifndef CN3D_SEQUENCE_VIEWER__HPP
#define CN3D_SEQUENCE_VIEWER__HPP

#include <corelib/ncbistl.hpp>

#include <list>

#include "cn3d/viewer_base.hpp"


BEGIN_SCOPE(Cn3D)

class Sequence;
class AlignmentManager;
class BlockMultipleAlignment;
class SequenceViewerWindow;

class SequenceViewer : public ViewerBase
{
    friend class SequenceViewerWindow;

public:

    SequenceViewer(AlignmentManager *alnMgr);
    ~SequenceViewer(void);

    void Refresh(void);

    // to create displays from unaligned sequence(s), or multiple alignment
    typedef std::list < const Sequence * > SequenceList;
    void DisplaySequences(const SequenceList *sequenceList);
    void DisplayAlignment(BlockMultipleAlignment *multipleAlignment);

    // functions to save edited data
    void SaveDialog(void);
    void SaveAlignment(void);

private:

    AlignmentManager *alignmentManager;
    SequenceViewerWindow *sequenceWindow;

    void CreateSequenceWindow(void);
};

END_SCOPE(Cn3D)

#endif // CN3D_SEQUENCE_VIEWER__HPP
