#ifndef UTIL_IMAGE__IMAGE_IO_GIF__HPP
#define UTIL_IMAGE__IMAGE_IO_GIF__HPP

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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *    CImageIOGif -- interface class for reading/writing CompuServ GIF files
 */

#include "image_io_handler.hpp"
#include <string>

// global-space predeclaration
struct GifFileType;


BEGIN_NCBI_SCOPE

class CImage;


//
// class CImageIOGif
// This is an internal class for handling GIF format files.  It relies on
// libgif / libungif for GIF encoding, and will support compression only if
// that library does
//

class CImageIOGif : public CImageIOHandler
{
public:

    // read GIF images from files
    CImage* ReadImage(const string& file);
    CImage* ReadImage(const string& file,
                      size_t x, size_t y, size_t w, size_t h);

    // write images to file in GIF format
    void WriteImage(const CImage& image, const string& file);
    void WriteImage(const CImage& image, const string& file,
                    size_t x, size_t y, size_t w, size_t h);
private:

    // read a single line from the file
    void x_ReadLine(GifFileType* fp, unsigned char* data);

    // unpack a single row of data
    void x_UnpackData(GifFileType* fp,
                      const unsigned char* from_data,
                      unsigned char* to_data);

};


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2003/06/03 15:17:13  dicuccio
 * Initial revision of image library
 *
 * ===========================================================================
 */

#endif  // UTIL_IMAGE__IMAGE_IO_GIF__HPP
