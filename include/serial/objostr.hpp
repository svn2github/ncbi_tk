#ifndef OBJOSTR__HPP
#define OBJOSTR__HPP

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
* Author: Eugene Vasilchenko
*
* File Description:
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.13  1999/07/09 16:32:53  vasilche
* Added OCTET STRING write/read.
*
* Revision 1.12  1999/07/07 21:15:03  vasilche
* Cleaned processing of string types (string, char*, const char*).
*
* Revision 1.11  1999/07/02 21:31:46  vasilche
* Implemented reading from ASN.1 binary format.
*
* Revision 1.10  1999/07/01 17:55:20  vasilche
* Implemented ASN.1 binary write.
*
* Revision 1.9  1999/06/30 16:04:30  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.8  1999/06/24 14:44:40  vasilche
* Added binary ASN.1 output.
*
* Revision 1.7  1999/06/17 20:42:02  vasilche
* Fixed storing/loading of pointers.
*
* Revision 1.6  1999/06/16 20:35:23  vasilche
* Cleaned processing of blocks of data.
* Added input from ASN.1 text format.
*
* Revision 1.5  1999/06/15 16:20:03  vasilche
* Added ASN.1 object output stream.
*
* Revision 1.4  1999/06/10 21:06:40  vasilche
* Working binary output and almost working binary input.
*
* Revision 1.3  1999/06/07 19:30:17  vasilche
* More bug fixes
*
* Revision 1.2  1999/06/04 20:51:35  vasilche
* First compilable version of serialization.
*
* Revision 1.1  1999/05/19 19:56:26  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/serialdef.hpp>
#include <serial/typeinfo.hpp>
#include <serial/objlist.hpp>
#include <map>

BEGIN_NCBI_SCOPE

class CMemberId;

class CObjectOStream
{
public:
    typedef unsigned TIndex;

    virtual ~CObjectOStream(void);

    // root writer
    virtual void Write(TConstObjectPtr object, TTypeInfo typeInfo);

    // std C types readers
    virtual void WriteStd(const char& data) = 0;
    virtual void WriteStd(const unsigned char& data) = 0;
    virtual void WriteStd(const signed char& data) = 0;
    virtual void WriteStd(const short& data) = 0;
    virtual void WriteStd(const unsigned short& data) = 0;
    virtual void WriteStd(const int& data) = 0;
    virtual void WriteStd(const unsigned int& data) = 0;
    virtual void WriteStd(const long& data) = 0;
    virtual void WriteStd(const unsigned long& data) = 0;
    virtual void WriteStd(const float& data) = 0;
    virtual void WriteStd(const double& data) = 0;

    virtual void WriteStr(const string& data);
    virtual void WriteStr(const char* const& data);
    virtual void WriteStr(char* const& data);

    // object level writers
    void WriteExternalObject(TConstObjectPtr object, TTypeInfo typeInfo);
    // type info writers
    virtual void WritePointer(TConstObjectPtr object, TTypeInfo typeInfo);

    class Member {
    public:
        Member(CObjectOStream& out, const CMemberId& member)
            : m_Out(out)
            {
                out.StartMember(*this, member);
            }
        ~Member(void)
            {
                m_Out.EndMember(*this);
            }
    private:
        CObjectOStream& m_Out;
    };
    // block interface
    class Block {
    public:
        Block(CObjectOStream& out);
        Block(CObjectOStream& out, unsigned size);
        Block(CObjectOStream& out, bool randomOrder);
        Block(CObjectOStream& out, bool randomOrder, unsigned size);
        ~Block(void);

        void Next(void);

        bool Fixed(void) const
            {
                return m_Fixed;
            }
        bool RandomOrder(void) const
            {
                return m_RandomOrder;
            }

        unsigned GetNextIndex(void) const
            {
                return m_NextIndex;
            }

        unsigned GetIndex(void) const
            {
                return GetNextIndex() - 1;
            }

        bool First(void) const
            {
                return GetNextIndex() == 0;
            }

        unsigned GetSize(void) const
            {
                return m_Size;
            }

    protected:
        CObjectOStream& m_Out;

        void IncIndex(void)
            {
                ++m_NextIndex;
            }

    private:
        // to prevent copying
        Block(const Block&);
        Block& operator=(const Block&);
        // to prevent allocation in heap
        void* operator new(size_t size);
        friend class CObjectOStream;

        bool m_Fixed;
        bool m_RandomOrder;
        unsigned m_NextIndex;
        unsigned m_Size;
    };
	class ByteBlock {
	public:
		ByteBlock(CObjectOStream& out, size_t length)
			: m_Out(out), m_Length(length)
		{
			out.Begin(*this);
		}
		~ByteBlock(void)
		{
			if ( m_Length != 0 )
				THROW1_TRACE(runtime_error, "not all bytes written");
			m_Out.End(*this);
		}

		size_t GetLength(void) const
		{
			return m_Length;
		}

		void Write(const void* bytes, size_t length)
		{
			if ( length > m_Length )
				THROW1_TRACE(runtime_error, "too many bytes written");
			m_Out.WriteBytes(*this, static_cast<const char*>(bytes), length);
			m_Length -= length;
		}

	private:
		CObjectOStream& m_Out;
		size_t m_Length;
	};
    
protected:
    // block interface
    friend class Block;
    friend class Member;
	friend class ByteBlock;
    static void SetNonFixed(Block& block)
        {
            block.m_Fixed = false;
            block.m_Size = 0;
        }
    virtual void FBegin(Block& block);
    virtual void VBegin(Block& block);
    virtual void FNext(const Block& block);
    virtual void VNext(const Block& block);
    virtual void FEnd(const Block& block);
    virtual void VEnd(const Block& block);
    // write member name
    virtual void StartMember(Member& member, const CMemberId& id) = 0;
    virtual void EndMember(const Member& member);
	// write byte blocks
	virtual void Begin(const ByteBlock& block);
	virtual void WriteBytes(const ByteBlock& block, const char* bytes, size_t length) = 0;
	virtual void End(const ByteBlock& block);

    // low level writers
    virtual void WriteMemberPrefix(COObjectInfo& info);
    virtual void WriteMemberSuffix(COObjectInfo& info);
    virtual void WriteNullPointer(void) = 0;
    virtual void WriteObjectReference(TIndex index) = 0;
    virtual void WriteThisTypeReference(TTypeInfo typeInfo);
    virtual void WriteOtherTypeReference(TTypeInfo typeInfo) = 0;
    virtual void WriteString(const string& str) = 0;
	virtual void WriteCString(const char* str);

    COObjectList m_Objects;
    void RegisterObject(CORootObjectInfo& info)
        { m_Objects.RegisterObject(info); }

    void WriteData(TConstObjectPtr object, TTypeInfo typeInfo)
        {
            typeInfo->WriteData(*this, object);
        }
};

//#include <serial/objostr.inl>

END_NCBI_SCOPE

#endif  /* OBJOSTR__HPP */
