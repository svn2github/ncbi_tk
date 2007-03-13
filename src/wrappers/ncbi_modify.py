# $Id$
# ===========================================================================
#
#                            PUBLIC DOMAIN NOTICE
#               National Center for Biotechnology Information
#
#  This software/database is a "United States Government Work" under the
#  terms of the United States Copyright Act.  It was written as part of
#  the author's official duties as a United States Government employee and
#  thus cannot be copyrighted.  This software/database is freely available
#  to the public for use. The National Library of Medicine and the U.S.
#  Government have not placed any restriction on its use or reproduction.
#
#  Although all reasonable efforts have been taken to ensure the accuracy
#  and reliability of the software and data, the NLM and the U.S.
#  Government do not and cannot warrant the performance or results that
#  may be obtained by using this software or data. The NLM and the U.S.
#  Government disclaim all warranties, express or implied, including
#  warranties of performance, merchantability or fitness for any particular
#  purpose.
#
#  Please cite the author in any work or product based on this material.
#
# ===========================================================================
#
# Authors:  Josh Cherry
#
# File Description:  Performs various special-purpose modifications of
#                    SWIG preprocessor output
#

# As a last resort for dealing with some problems wrapping with SWIG,
# we modify the SWIG preprocessor output before feeding it back to SWIG.

import os, sys, re

def Modify(s):
    trouble = '''    template<typename T>
    class SOptional {
    public:
        /// Constructor.
        SOptional()
            : m_IsSet(false), m_Value(T())
        {
        }
        
        /// Check whether the value has been set.
        bool Have()
        {
            return m_IsSet;
        }
        
        /// Get the value.
        T Get()
        {
            return m_Value;
        }
        
        /// Assign the field from another optional field.
        SOptional<T> & operator=(const T & x)
        {
            m_IsSet = true;
            m_Value = x;
            return *this;
        }
        
    private:
        /// True if the value has been specified.
        bool m_IsSet;
        
        /// The value itself, valid only if m_IsSet is true.
        T    m_Value;
    };
'''
    s = re.sub(re.escape(trouble), '// template class SOptimal deleted\n', s)

    # Part of hit_filter.hpp causes SWIG parse error
    begin_trouble = \
        '    // merging of abutting hits sharing same ids and subject strand'
    end_trouble = '\n    }\n'
    begin_index = s.index(begin_trouble)
    end_index   = s.index(end_trouble, begin_index) + len(end_trouble)
    s = s[:begin_index] + '// sx_TestAndMerge deleted\n' + s[end_index:]
    
    # In biotree.hpp, nested class inheriting from template causes trouble.
    # Hide the inheritance from SWIG.
    s = re.sub(re.escape('class CBioNode : public CTreeNode<TBioNode>'),
               'class CBioNode', s)
    
    return s
