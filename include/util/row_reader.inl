#ifndef UTIL___ROW_READER__INL
#define UTIL___ROW_READER__INL

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
* Authors: Design and advice - Denis Vakatov
*          Implementation and critique - Sergey Satskiy
*
* Credits: Alex Astashyn
*          (GetFieldValueConverted)
*
* File Description:
*   CRowReader utility types, classes, functions
** ===========================================================================
*/


#ifndef UTIL___ROW_READER_INCLUDE__INL
#  error "The row_reader.inl must only be included in row_reader.hpp"
#endif


/// Delimited stream traits use the ERR_Action members to instruct what should
/// be done next. These values are returned by various trait callbacks.
enum ERR_Action {
    /// Skip this line
    eRR_Skip,
    /// Continue processing this line, in full
    eRR_Continue_Data,
    /// Continue processing this line but skip Tokenize() and Translate()
    /// @note
    ///  Not allowed for Tokenize() and Translate() returns
    eRR_Continue_Comment,
    /// Continue processing this line but skip Tokenize() and Translate()
    /// @note
    ///  Not allowed for Tokenize() and Translate() returns
    eRR_Continue_Metadata,
    /// Continue processing this line but skip Tokenize() and Translate()
    /// @note
    ///  not allowed for Tokenize() and Translate() returns
    eRR_Continue_Invalid,
    /// Stop processing the stream
    eRR_Interrupt
};


/// The Translate() callback result. It is used to translate field values.
enum ERR_TranslationResult {
    eRR_UseOriginal,   ///< No translation done
    eRR_Translated,    ///< The value has been translated to another string
    eRR_Null           ///< The value has been translated to NULL
};



/// Miscellaneous trait-specific flags
/// It is used when a row stream should be built on a file. The file will be
/// opened with the mode provided by the traits, see
///   TTraitsFlags GetFlags(void) const
/// member.
enum ERR_TraitsFlags {
    fRR_OpenFile_AsBinary = (0 << 1),  ///< open file in a binary mode
    fRR_Default           = 0          ///< open file in a text mode
};
typedef int TTraitsFlags;  ///< Bit-wise OR of ERR_TraitsFlags


/// Potentially disruptive events
/// CRowReader passes such events to the Traits via OnEvent() callback
enum ERR_Event {
    eRR_Event_SourceEOF,   ///< Data source has hit EOF
    eRR_Event_SourceError  ///< Data source has hit an error on read
};


/// How to react to the potentially disruptive events
/// @sa OnEvent()
enum ERR_EventAction {
    eRR_EventAction_Default,  ///< Do some default action
    eRR_EventAction_Stop,     ///< Stop further processing
    eRR_EventAction_Continue  ///< Resume processing
};


/// The class represents the current delimited row stream context
class CRR_Context
{
public:
    virtual string Serialize() const
    {
        string context = "Row reader context: ";
        if ( !m_SourceName.empty() )
            context += "Source name: " + m_SourceName + "; ";

        if (m_LinesAlreadyRead) {
            context += "Last read line position in the stream: " +
                       NStr::NumericToString(m_CurrentLinePos) + "; "
                       "Last read line number: " +
                       NStr::NumericToString(m_CurrentLineNo) + "; ";
        } else {
            if (!m_ReachedEnd)
                context += "Position in the stream: " +
                           NStr::NumericToString(m_CurrentLinePos) + "; "
                           "No lines read yet; ";
        }

        if (m_RawDataAvailable)
            context += "Raw line data: '" + m_RawData + "'; ";
        else
            context += "Raw line data are not available; ";

        if (m_ReachedEnd)
            context += "Stream has reached end";
        else
            context += "Stream has not reached end yet";
        return context;
    }

    virtual ~CRR_Context()
    {}

    CRR_Context() :
        m_LinesAlreadyRead(false),
        m_CurrentLineNo(0), m_CurrentLinePos(0),
        m_RawDataAvailable(false),
        m_ReachedEnd(false)
    {}

    CRR_Context(const string& sourcename,
                bool lines_already_read,
                TLineNo line_no,
                TStreamPos current_line_pos,
                bool raw_data_available,
                const string& raw_data,
                bool reached_end) :
        m_SourceName(sourcename), m_LinesAlreadyRead(lines_already_read),
        m_CurrentLineNo(line_no), m_CurrentLinePos(current_line_pos),
        m_RawDataAvailable(raw_data_available), m_RawData(raw_data),
        m_ReachedEnd(reached_end)
    {}

    // Deriving classes must implement their own version of this member to
    // have the exceptions working properly
    virtual CRR_Context* Clone(void) const
    {
        return new CRR_Context(m_SourceName, m_LinesAlreadyRead,
                               m_CurrentLineNo, m_CurrentLinePos,
                               m_RawDataAvailable, m_RawData,
                               m_ReachedEnd);
    }

public:
    /// Name of the data source, such as:
    /// - file name if the stream was constructed on a file using SetDataSource()
    /// - URL if the stream was constructed on some other resource
    /// - "User Stream" stream was passed in using SetDataSource()
    string      m_SourceName;

    // true if at least one line has been successfully read from the stream
    bool        m_LinesAlreadyRead;
    TLineNo     m_CurrentLineNo;    // 0-based
    TStreamPos  m_CurrentLinePos;   // 0-based

    // true if the raw data has been read successfully
    bool        m_RawDataAvailable;
    string      m_RawData;

    bool        m_ReachedEnd;
};


/// Exception to be used throughout API
class CRowReaderException : public CException
{
public:
    enum EErrCode {
        eUnexpectedRowType,
        eStreamFailure,
        eFieldNoNotFound,
        eDereferencingEndIterator,
        eAdvancingEndIterator,
        eFileNotFound,
        eNoReadPermissions,
        eInvalidAction,
        eLineProcessing,
        eEndIteratorRowAccess,
        eFieldNoOutOfRange,
        eFieldAccess,
        eFieldNameNotFound,
        eFieldMetaInfoAccess,
        eFieldConvert,
        eNullField,
        eValidating,
        eNonEndIteratorCompare,
        eIteratorWhileValidating
    };

    CRowReaderException(
            const CDiagCompileInfo& info,
            const CException* prev_exception, EErrCode err_code,
            const string& message, CRR_Context* ctxt,
            EDiagSev severity = eDiag_Error) :
        CException(info, prev_exception, message, severity),
        m_Context(ctxt)
    NCBI_EXCEPTION_DEFAULT_IMPLEMENTATION(CRowReaderException, CException);

    virtual const char *  GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
            case eUnexpectedRowType:
                return "eUnexpectedRowType";
            case eStreamFailure:
                return "eStreamFailure";
            case eFieldNoNotFound:
                return "eFieldNoNotFound";
            case eDereferencingEndIterator:
                return "eDereferencingEndIterator";
            case eAdvancingEndIterator:
                return "eAdvancingEndIterator";
            case eFileNotFound:
                return "eFileNotFound";
            case eNoReadPermissions:
                return "eNoReadPermissions";
            case eInvalidAction:
                return "eInvalidAction";
            case eLineProcessing:
                return "eLineProcessing";
            case eEndIteratorRowAccess:
                return "eEndIteratorRowAccess";
            case eFieldNoOutOfRange:
                return "eFieldNoOutOfRange";
            case eFieldAccess:
                return "eFieldAccess";
            case eFieldNameNotFound:
                return "eFieldNameNotFound";
            case eFieldMetaInfoAccess:
                return "eFieldMetaInfoAccess";
            case eFieldConvert:
                return "eFieldConvert";
            case eNullField:
                return "eNullField";
            case eValidating:
                return "eValidating";
            case eNonEndIteratorCompare:
                return "eNonEndIteratorCompare";
            case eIteratorWhileValidating:
                return "eIteratorWhileValidating";
            default:
                return CException::GetErrCodeString();
        }
    }

    virtual void ReportExtra(ostream& out) const
    {
        if (m_Context)
            out << m_Context->Serialize();
        else
            out << "No context available";
    }

public:
    CRR_Context* GetContext(void) const
    {
        return m_Context.get();
    }

    void SetContext(CRR_Context* ctxt)
    {
        m_Context.reset(ctxt);
    }

protected:
    virtual void x_Assign(const CException& src)
    {
        CException::x_Assign(src);

        const CRowReaderException& other =
            dynamic_cast<const CRowReaderException&>(src);

        if (other.m_Context)
            m_Context.reset(other.m_Context->Clone());
        else
            m_Context.reset(nullptr);
    }

private:
    unique_ptr<CRR_Context> m_Context;
};




// Utility class merely for a scope
class CRR_Util
{
public:
    // Converts a callback provided action to a string
    static string ERR_ActionToString(ERR_Action action)
    {
        switch (action) {
            case eRR_Skip:              return "eRR_Skip";
            case eRR_Continue_Data:     return "eRR_Continue_Data";
            case eRR_Continue_Comment:  return "eRR_Continue_Comment";
            case eRR_Continue_Metadata: return "eRR_Continue_Metadata";
            case eRR_Continue_Invalid:  return "eRR_Continue_Invalid";
            case eRR_Interrupt:         return "eRR_Interrupt";
        }
        return "unknown";
    }

    // Converts the callback returned action to a row type
    static ERR_RowType ActionToRowType(ERR_Action action)
    {
        switch (action) {
            case eRR_Continue_Data:     return eRR_Data;
            case eRR_Continue_Comment:  return eRR_Comment;
            case eRR_Continue_Metadata: return eRR_Metadata;
            case eRR_Continue_Invalid:  return eRR_Invalid;
            default:
                NCBI_THROW2(CRowReaderException, eUnexpectedRowType,
                            "Unexpected action to convert to a row type",
                            nullptr);
        }
    }

    // Checks the file existance and read permissions
    static void CheckExistanceAndPermissions(const string& sourcename)
    {
        CFile file(sourcename);
        if ( !file.Exists() )
            NCBI_THROW2(CRowReaderException, eFileNotFound,
                        "File " + sourcename + " is not found",
                        nullptr);
        if ( !file.CheckAccess(CDirEntry::fRead) )
            NCBI_THROW2(CRowReaderException, eNoReadPermissions,
                        "No read permissions for file " + sourcename,
                        nullptr);
    }

    // Utility functions to implement field value conversions
    static void GetFieldValueConverted(const string& str_value,
                                       CTime& converted)
    {
        converted = CTime(str_value);
    }

    static void GetFieldValueConverted(const string& str_value,
                                       string& converted)
    {
        converted = str_value;
    }

    static void GetFieldValueConverted(const string& str_value,
                                       bool& converted)
    {
        converted = NStr::StringToBool(str_value);
    }

    // Conversion implementation respecting overflow
    // and covering all the arithmetic types
    template<typename T,
             typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
    static void GetFieldValueConverted(const string& str_value, T& converted)
    {
        errno = 0;
        try {
            if (!NStr::StringToNumeric(str_value, &converted)
                || errno != 0)
                throw;
        } catch (const CException& exc) {
            NCBI_RETHROW2(exc, CRowReaderException,
                          eFieldConvert, "Cannot convert field value '" +
                          str_value + "' to " + typeid(T).name(), nullptr);
        } catch (const exception& exc) {
            NCBI_THROW2(CRowReaderException, eFieldConvert,
                        "Cannot convert field value '" +
                        str_value + "' to " + typeid(T).name() + ": " +
                        exc.what(), nullptr);
        } catch (...) {
            NCBI_THROW2(CRowReaderException, eFieldConvert,
                        "Unknown error while converting field value '" +
                        str_value + "' to " + typeid(T).name(), nullptr);
        }
    }
};


// Stores the fields meta information: field names and types
template <typename TTraits>
class CRR_MetaInfo : public CObject
{
public:
    CRR_MetaInfo()
    {
        m_FieldsInfo.reserve(64);
    }

    CRR_MetaInfo(const CRR_MetaInfo& other)
    {
        Clear();
        m_FieldNamesIndex = other.m_FieldNamesIndex;

        m_FieldsInfo.reserve(other.m_FieldsInfo.size());
        for (size_t  k = 0; k < other.m_FieldsInfo.size(); ++k) {
            m_FieldsInfo.push_back(other.m_FieldsInfo[k]);
            if (other.m_FieldsInfo[k].m_NameInitialized) {
                auto  it = m_FieldNamesIndex.find(
                                *other.m_FieldsInfo[k].m_FieldName);
                m_FieldsInfo[k].m_FieldName = &it->first;
            }
        }
    }

public:
    void Clear()
    {
        m_FieldsInfo.clear();
        m_FieldNamesIndex.clear();
    }

    void SetFieldName(TFieldNo field, const string& name)
    {
        if (field >= m_FieldsInfo.size())
            m_FieldsInfo.resize(field + 1);

        if (m_FieldsInfo[field].m_NameInitialized) {
            if (name == *m_FieldsInfo[field].m_FieldName)
                return;

            if (x_UpdateNameRef(field) == 1)
                m_FieldNamesIndex.erase(*m_FieldsInfo[field].m_FieldName);
        }

        auto inserted = m_FieldNamesIndex.insert(make_pair(name, field));
        m_FieldsInfo[field].m_NameInitialized = true;
        m_FieldsInfo[field].m_FieldName = &inserted.first->first;
    }

    void SetFieldType(TFieldNo field, ERR_FieldType type)
    {
        if (field >= m_FieldsInfo.size())
            m_FieldsInfo.resize(field + 1);
        m_FieldsInfo[field].m_FieldType = type;
        m_FieldsInfo[field].m_TypeInitialized = true;
    }

    void SetFieldTypeEx(TFieldNo                             field,
                        ERR_FieldType                        type,
                        typename TTraits::TExtendedFieldType extended_type)
    {
        if (field >= m_FieldsInfo.size())
            m_FieldsInfo.resize(field + 1);
        m_FieldsInfo[field].m_FieldType = type;
        m_FieldsInfo[field].m_TypeInitialized = true;
        m_FieldsInfo[field].m_FieldExtType = extended_type;
        m_FieldsInfo[field].m_ExtTypeInitialized = true;
    }

    const string& GetFieldName(TFieldNo field) const
    {
        bool      found(field < m_FieldsInfo.size());
        if (found && !m_FieldsInfo[field].m_NameInitialized)
            found = false;
        if (!found)
            NCBI_THROW2(CRowReaderException, eFieldNoNotFound,
                        "Field name is not found for the field number " +
                        NStr::NumericToString(field), nullptr);
        return *m_FieldsInfo[field].m_FieldName;
    }

    ERR_FieldType GetFieldType(TFieldNo field) const
    {
        bool      found(field < m_FieldsInfo.size());
        if (found && !m_FieldsInfo[field].m_TypeInitialized)
            found = false;
        if (!found)
            NCBI_THROW2(CRowReaderException, eFieldNoNotFound,
                        "Field type is not found for the field number " +
                        NStr::NumericToString(field), nullptr);
        return m_FieldsInfo[field].m_FieldType;
    }

    typename TTraits::TExtendedFieldType
    GetExtendedFieldType(TFieldNo field) const
    {
        bool      found(field < m_FieldsInfo.size());
        if (found && !m_FieldsInfo[field].m_ExtTypeInitialized)
            found = false;
        if (!found)
            NCBI_THROW2(CRowReaderException, eFieldNoNotFound,
                        "Field extended type is not found for the field "
                        "number " + NStr::NumericToString(field), nullptr);
        return m_FieldsInfo[field].m_FieldExtType;
    }

    TFieldNo GetFieldIndexByName(const string& field) const
    {
        auto it = m_FieldNamesIndex.find(field);
        if (it == m_FieldNamesIndex.end())
            NCBI_THROW2(CRowReaderException, eFieldNoNotFound,
                        "Unknown field name '" + field + "'", nullptr);
        return it->second;
    }

private:
    size_t  x_UpdateNameRef(TFieldNo  field)
    {
        const string* name_ptr = m_FieldsInfo[field].m_FieldName;
        size_t        use_count = 0;
        TFieldNo      candidate = 0;

        for (TFieldNo index = 0; index < m_FieldNamesIndex.size(); ++index) {
            if (m_FieldsInfo[index].m_NameInitialized) {
                if (m_FieldsInfo[index].m_FieldName == name_ptr) {
                    ++use_count;
                    if (index != field)
                        candidate = index;
                }
            }
        }

        if (use_count > 1)
            m_FieldNamesIndex[*name_ptr] = candidate;
        return use_count;
    }

private:
    struct SMetainfo
    {
        SMetainfo() :
            m_FieldName(nullptr),
            m_NameInitialized(false), m_TypeInitialized(false),
            m_ExtTypeInitialized(false)
        {}

        const string *                          m_FieldName;
        ERR_FieldType                           m_FieldType;
        typename TTraits::TExtendedFieldType    m_FieldExtType;
        bool                                    m_NameInitialized;
        bool                                    m_TypeInitialized;
        bool                                    m_ExtTypeInitialized;
    };

    map<string, TFieldNo>   m_FieldNamesIndex;
    vector<SMetainfo>       m_FieldsInfo;
};


// Auxiliary structure to store a current stream info and a next stream info in
// case of SetDataSource() calls
struct SRR_SourceInfo
{
    SRR_SourceInfo():
         m_Stream(nullptr), m_StreamOwner(false)
    {}

    SRR_SourceInfo(CNcbiIstream* s, const string& sourcename, bool owner) :
        m_Stream(s), m_Sourcename(sourcename), m_StreamOwner(owner)
    {}

    ~SRR_SourceInfo()
    {
        Clear();
    }

    void Clear(void)
    {
        if (m_StreamOwner)
            delete m_Stream;

        m_Stream = nullptr;
        m_Sourcename.clear();
        m_StreamOwner = false;
    }

    CNcbiIstream*   m_Stream;
    string          m_Sourcename;
    bool            m_StreamOwner;
};



/// This macro can be used in the traits class declaration to provide
/// standard typedefs and methods to bind to the traits' "parent stream"
#define RR_TRAITS_PARENT_STREAM(TTraits) \
public:  \
    /* Convenience stream type shorthand */ \
    typedef CRowReader<TTraits> TStreamType;  \
protected:  \
    TStreamType& GetMyStream(void) const { return *m_MyStream; } \
private: \
    friend TStreamType; \
    /* The CRowReader objects calls this function */ \
    void x_SetMyStream(TStreamType* my_stream) { m_MyStream = my_stream; } \
    TStreamType* m_MyStream



#endif  /* UTIL___ROW_READER__INL */
