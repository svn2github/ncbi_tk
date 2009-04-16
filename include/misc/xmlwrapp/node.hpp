/*
 * Copyright (C) 2001-2003 Peter J Jones (pjones@pmade.org)
 *               2009      Vaclav Slavik <vslavik@fastmail.fm>
 * All Rights Reserved
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * $Id$ 
 * NOTE: This file was modified from its original version 0.6.0
 *       to fit the NCBI C++ Toolkit build framework and
 *       API and functionality requirements. 
 */

/** @file
 * This file contains the definition of the xml::node class.
**/

#ifndef _xmlwrapp_node_h_
#define _xmlwrapp_node_h_

// xmlwrapp includes
#include <misc/xmlwrapp/xml_init.hpp>
#include <misc/xmlwrapp/ns.hpp>

// hidden stuff
#include <misc/xmlwrapp/impl/_cbfo.hpp>

// standard includes
#include <cstddef>
#include <iosfwd>
#include <string>
#include <vector>

namespace xml {

// forward declarations
class document;
class attributes;
class nodes_view;
class const_nodes_view;

namespace impl {
class node_iterator;
class iter_advance_functor;
struct node_impl;
struct doc_impl;
struct nipimpl;
struct node_cmp;
}

/**
 * The xml::node class is used to hold information about one XML node. This
 * includes the name of the node, the namespace of the node and attributes
 * for the node. It also has an iterator whereby you can get to the children
 * nodes.
 *
 * It should be noted that any member function that returns a const char*
 * returns a temporary value. The pointer that is returned will change with
 * ANY operation to the xml::node. If you need the data to stick around a
 * little longer you should put it inside a std::string.
**/
class node {
public:
    /// size type
    typedef std::size_t size_type;

    typedef std::vector<xml::ns>  ns_list_type;  //< type for holding XML namespaces

    /// enum for the different types of XML nodes
    enum node_type {
	type_element,		///< XML element such as "<chapter/>"
	type_text,		///< Text node
	type_cdata,		///< <![CDATA[text]]>
	type_pi,		///< Processing Instruction
	type_comment,		///< XML comment
	type_entity,		///< Entity as in &amp;amp;
	type_entity_ref,	///< Entity ref
	type_xinclude,		///< <xi:include/> node
	type_document,		///< Document node
	type_document_type,	///< DOCTYPE node
	type_document_frag,	///< Document Fragment
	type_notation,		///< Notation
	type_dtd,		///< DTD node
	type_dtd_element,	///< DTD <!ELEMENT> node
	type_dtd_attribute,	///< DTD <!ATTRLIST> node
	type_dtd_entity,	///< DTD <!ENTITY>
	type_dtd_namespace	///< ?
    };

    /// enum for policies of adding namespace definitions
    enum ns_definition_adding_type {
        type_replace_if_exists, ///< replace URI if ns with the same prefix exists
        type_throw_if_exists    ///< throw exception if ns with the same prefix exists
    };

    /**
     * Helper struct for creating a xml::node of type_cdata.
     *
     * @code
     * xml::node mynode(xml::node::cdata("This is a CDATA section"));
     * @endcode
     */
    struct cdata {
	explicit cdata (const char *text) : t(text) { }
	const char *t;
    };

    /**
     * Helper struct for creating a xml::node of type_comment.
     *
     * @code
     * xml::node mynode(xml::node::comment("This is an XML comment"));
     * @endcode
     */
    struct comment {
	explicit comment (const char *text) : t(text) { }
	const char *t;
    };

    /**
     * Helper struct for creating a xml::node of type_pi.
     *
     * @code
     * xml::node mynode(xml::node::pi("xslt", "stylesheet=\"test.xsl\""));
     * @endcode
     */
    struct pi {
	explicit pi (const char *name, const char *content=0) : n(name), c(content) { }
	const char *n, *c;
    };

    /**
     * Helper struct for creating a xml::node of type_text.
     *
     * @code
     * xml::node mynode(xml::node::text("This is an XML text fragment"));
     * @endcode
     */
    struct text {
	explicit text (const char *text) : t(text) { }
	const char *t;
    };

    //####################################################################
    /** 
     * Construct a new blank xml::node.
     *
     * @author Peter Jones
    **/
    //####################################################################
    node (void);

    //####################################################################
    /** 
     * Construct a new xml::node and set the name of the node.
     *
     * @param name The name of the new node.
     * @author Peter Jones
    **/
    //####################################################################
    explicit node (const char *name);

    //####################################################################
    /** 
     * Construct a new xml::node given a name and content. The content will
     * be used to create a new child text node.
     *
     * @param name The name of the new element.
     * @param content The text that will be used to create a child node.
     * @author Peter Jones
    **/
    //####################################################################
    node (const char *name, const char *content);

    //####################################################################
    /** 
     * Construct a new xml::node that is of type_cdata. The cdata_info
     * parameter should contain the contents of the CDATA section.
     *
     * @note Sample Use Example:
     * @code 
     * xml::node mynode(xml::node::cdata("This is a CDATA section"));
     * @endcode
     *
     * @param cdata_info A cdata struct that tells xml::node what the content will be.
     * @author Peter Jones
    **/
    //####################################################################
    explicit node (cdata cdata_info);

    //####################################################################
    /** 
     * Construct a new xml::node that is of type_comment. The comment_info
     * parameter should contain the contents of the XML comment.
     *
     * @note Sample Use Example:
     * @code
     * xml::node mynode(xml::node::comment("This is an XML comment"));
     * @endcode
     *
     * @param comment_info A comment struct that tells xml::node what the comment will be.
     * @author Peter Jones
    **/
    //####################################################################
    explicit node (comment comment_info);

    //####################################################################
    /** 
     * Construct a new xml::node that is of type_pi. The pi_info parameter
     * should contain the name of the XML processing instruction (PI), and
     * optionally, the contents of the XML PI.
     *
     * @note Sample Use Example:
     * @code
     * xml::node mynode(xml::node::pi("xslt", "stylesheet=\"test.xsl\""));
     * @endcode
     *
     * @param pi_info A pi struct that tells xml::node what the name and contents of the XML PI are.
     * @author Peter Jones
    **/
    //####################################################################
    explicit node (pi pi_info);

    //####################################################################
    /** 
     * Construct a new xml::node that is of type_text. The text_info
     * parameter should contain the text.
     *
     * @note Sample Use Example:
     * @code
     * xml::node mynode(xml::node::text("This is XML text"));
     * @endcode
     *
     * @param text_info A text struct that tells xml::node what the text will be.
     * @author Vaclav Slavik
    **/
    //####################################################################
    explicit node (text text_info);

    //####################################################################
    /** 
     * Construct a new xml::node by copying another xml::node.
     *
     * @param other The other node to copy.
     * @author Peter Jones
    **/
    //####################################################################
    node (const node &other);

    //####################################################################
    /** 
     * Make this node equal to some other node via assignment.
     *
     * @param other The other node to copy.
     * @return A reference to this node.
     * @author Peter Jones
    **/
    //####################################################################
    node& operator= (const node &other);

    //####################################################################
    /** 
     * Class destructor
     *
     * @author Peter Jones
    **/
    //####################################################################
    ~node (void);

    //####################################################################
    /** 
     * Set the name of this xml::node.
     *
     * @param name The new name for this xml::node.
     * @author Peter Jones
    **/
    //####################################################################
    void set_name (const char *name);

    //####################################################################
    /** 
     * Get the name of this xml::node.
     *
     * This function may change in the future to return std::string.
     * Feedback is welcome.
     *
     * @return The name of this node.
     * @author Peter Jones
    **/
    //####################################################################
    const char* get_name (void) const;

    //####################################################################
    /** 
     * Set the content of a node. If this node is an element node, this
     * function will remove all of its children nodes and replace them
     * with one text node set to the given string.
     *
     * @param content The content of the text node.
     * @author Peter Jones
    **/
    //####################################################################
    void set_content (const char *content);

    //####################################################################
    /** 
     * Get the content for this text node. If this node is not a text node
     * but it has children nodes that are text nodes, the contents of those
     * child nodes will be returned. If there is no content or these
     * conditions do not apply, zero will be returned.
     *
     * This function may change in the future to return std::string.
     * Feedback is welcome.
     *
     * @return The content or 0.
     * @author Peter Jones
    **/
    //####################################################################
    const char* get_content (void) const;

    //####################################################################
    /** 
     * Get this node's "type". You can use that information to know what you
     * can and cannot do with it.
     *
     * @return The node's type.
     * @author Peter Jones
    **/
    //####################################################################
    node_type get_type (void) const;

    //####################################################################
    /** 
     * Get the list of attributes. You can use the returned object to get
     * and set the attributes for this node. Make sure you use a reference
     * to this returned object, to prevent a copy.
     *
     * @return The xml::attributes object for this node.
     * @author Peter Jones
    **/
    //####################################################################
    xml::attributes& get_attributes (void);

    //####################################################################
    /** 
     * Get the list of attributes. You can use the returned object to get
     * the attributes for this node. Make sure you use a reference to this
     * returned object, to prevent a copy.
     *
     * @return The xml::attributes object for this node.
     * @author Peter Jones
    **/
    //####################################################################
    const xml::attributes& get_attributes (void) const;

    //####################################################################
    /**
     * Get the namespace of this xml::node. If a node has no namespace
     * then an object with both empty a prefix and uri is returned (the
     * is_void() method of the xml::ns object will return true
     * in this case)
     *
     * @param type The required type of namespace object (safe/unsafe).
     * @return The namespace of this node. Void namespace object if no
     *         namespace is associated.
     * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    xml::ns get_namespace (xml::ns::ns_safety_type type = xml::ns::type_safe_ns) const;

    //####################################################################
    /**
      * Get the namespaces defined at this xml::node.
      *
      * @param type The required type of namespace objects (safe/unsafe).
      * @return The namespace defined at this node.
      *         If no namespaces are defined then the container is empty
      *         If a default namespace is defined at the node then
      *         the first element in the corresponding container item
      *         is an empty string.
      * @author Sergey Satskiy, NCBI
     **/
    //####################################################################
    ns_list_type get_namespace_definitions (xml::ns::ns_safety_type type = xml::ns::type_safe_ns) const;

    //####################################################################
    /**
      * Set the node namespace.
      * The namespace is searched up in the hierarchy of nodes. If a namespace
      * with the given prefix is not found or uris of the found and the given
      * namespaceas are not matched then an exception is thrown.
      * A void namespace is treated as a namespace removal request (the same as
      * erase_namespace() call).
      *
      * @param name_space to be set to
      * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    void set_namespace (const xml::ns &name_space);

    //####################################################################
    /**
      * Set the node namespace.
      * The namespace is searched up in the hierarchy of nodes. If a namespace
      * with the given prefix is not found then an exception is thrown.
      *
      * @param prefix Namespace prefix. The NULL pointer and empty string
      *        are considered as a default namespace.
      * @author Sergey Satskiy, NCBI
     **/
    //####################################################################
    void set_namespace (const char *prefix);

    //####################################################################
    /**
      * Add namespace definition to the node.
      *
      * @param name_space The namespace definition to be added to the node.
      *           If the node already has a namespace definition with same
      *           prefix then its URI will be replaced with the new one.
      *           The hierarchy of nodes is walked down and if a namespace with
      *           the given prefix is found then it is replaced with the given
      *           namespace.
      * @param type replace or throw exception if a namespace definition exists
      *           with the same prefix
      * @return unsafe namespace
      * @author Sergey Satskiy, NCBI
     **/
    //####################################################################
    xml::ns add_namespace_definition (const xml::ns &name_space, ns_definition_adding_type type);

    //####################################################################
    /**
      * Add namespace definitions to the node.
      *
      * @param name_spaces The namespace definitions to be added to the node.
      *           If the node already has namespace definitions which
      *           prefixes are the same as those in the given list then
      *           the matched prefixes URIs will be replaced with the new ones.
      *           The hierarchy of nodes is walked down and if a namespace with
      *           the given prefix is found then it is replaced with the given
      *           namespace.
      * @param type replace or throw exception if a namespace definition exists
      *           with the same prefix
      * @author Sergey Satskiy, NCBI
     **/
    //####################################################################
    void add_namespace_definitions (const ns_list_type &name_spaces, ns_definition_adding_type type);

    //####################################################################
    /**
      * Remove the node namespace definition.
      *
      * @param prefix The prefix of the namespace to be removed from the node
      *               namespace definitions. If there is no such a namespace
      *               definition nothing will be removed.
      *               The NULL and pointers to the empty string are considered
      *               as a default namespace.
      * @author Sergey Satskiy, NCBI
     **/
    //####################################################################
    void erase_namespace_definition (const char *prefix);

    //####################################################################
    /**
      * Remove the node namespace.
      * The hierarchy of nodes is searched up and if a default namespace is
      * found then it is used as a new node namespace.
      *
      * @author Sergey Satskiy, NCBI
     **/
    //####################################################################
    void erase_namespace (void);

    //####################################################################
    /**
      * Look up a namespace for the given prefix.
      * The function walks the nodes hierarchy up and checks the nodes
      * defined namespaces prefixes. If the prefix matches the given then the
      * corresponding safe/unsafe ns object is provided.
      *
      * @param prefix Namespace prefix to be searched. NULL and "" are treated
      *               as a default namespace prefix.
      * @param type The required type of namespace object (safe/unsafe).
      * @return safe/unsafe namespace depending on the requested type.
      *         If a namespace is not found then a void namespace object is
      *         returned i.e. is_void() will return true.
      * @author Sergey Satskiy, NCBI
     **/
    //####################################################################
    xml::ns lookup_namespace (const char *prefix, xml::ns::ns_safety_type type = xml::ns::type_safe_ns) const;

    //####################################################################
    /**
     * Find out if this node is a text node or sometiming like a text node,
     * CDATA for example.
     *
     * @return True if this node is a text node; false otherwise.
     * @author Peter Jones
    **/
    //####################################################################
    bool is_text (void) const;

    //####################################################################
    /** 
     * Add a child xml::node to this node.
     *
     * @param child The child xml::node to add.
     * @author Peter Jones
    **/
    //####################################################################
    void push_back (const node &child);

    //####################################################################
    /** 
     * Swap this node with another one.
     *
     * @param other The other node to swap with.
     * @author Peter Jones
    **/
    //####################################################################
    void swap (node &other);

    class const_iterator; // forward declaration

    /**
     * The xml::node::iterator provides a way to access children nodes
     * similar to a standard C++ container. The nodes that are pointed to by
     * the iterator can be changed.
     */
    class iterator {
    public:
	typedef node value_type;
	typedef int difference_type;
	typedef value_type* pointer;
	typedef value_type& reference;
	typedef std::forward_iterator_tag iterator_category;

	iterator  (void) : pimpl_(0) {}
	iterator  (const iterator &other);
	iterator& operator= (const iterator& other);
	~iterator (void);

	reference operator*  (void) const;
	pointer   operator-> (void) const;

	/// prefix increment
	iterator& operator++ (void);

	/// postfix increment (avoid if possible for better performance)
	iterator  operator++ (int);

    bool operator==(const iterator& other) const
        { return get_raw_node() == other.get_raw_node(); }
    bool operator!=(const iterator& other) const
        { return !(*this == other); }

    private:
    impl::nipimpl *pimpl_;
	explicit iterator (void *data);
	void* get_raw_node (void) const;
	void swap (iterator &other);
	friend class node;
	friend class document;
	friend class const_iterator;
    };

    /**
     * The xml::node::const_iterator provides a way to access children nodes
     * similar to a standard C++ container. The nodes that are pointed to by
     * the const_iterator cannot be changed.
     */
    class const_iterator {
    public:
	typedef const node value_type;
	typedef int difference_type;
	typedef value_type* pointer;
	typedef value_type& reference;
	typedef std::forward_iterator_tag iterator_category;

	const_iterator  (void) : pimpl_(0) {}
	const_iterator  (const const_iterator &other);
	const_iterator  (const iterator &other);
	const_iterator& operator= (const const_iterator& other);
	~const_iterator (void);

	reference operator*  (void) const;
	pointer   operator-> (void) const;

	/// prefix increment
	const_iterator& operator++ (void);

	/// postfix increment (avoid if possible for better performance)
	const_iterator  operator++ (int);

    bool operator==(const const_iterator& other) const
        { return get_raw_node() == other.get_raw_node(); }
    bool operator!=(const const_iterator& other) const
        { return !(*this == other); }
    private:
    impl::nipimpl *pimpl_;
	explicit const_iterator (void *data);
	void* get_raw_node (void) const;
	void swap (const_iterator &other);
	friend class document;
	friend class node;
    };

    //####################################################################
    /** 
     * Returns the number of childer this nodes has. If you just want to
     * know how if this node has children or not, you should use
     * xml::node::empty() instead.
     *
     * @return The number of children this node has.
     * @author Peter Jones
    **/
    //####################################################################
    size_type size (void) const;

    //####################################################################
    /** 
     * Find out if this node has any children. This is the same as
     * xml::node::size() == 0 except it is much faster.
     *
     * @return True if this node DOES NOT have any children.
     * @return False if this node does have children.
     * @author Peter Jones
    **/
    //####################################################################
    bool empty (void) const;

    //####################################################################
    /** 
     * Get an iterator that points to the beginning of this node's children.
     *
     * @return An iterator that points to the beginning of the children.
     * @author Peter Jones
    **/
    //####################################################################
    iterator begin (void);

    //####################################################################
    /** 
     * Get a const_iterator that points to the beginning of this node's
     * children.
     *
     * @return A const_iterator that points to the beginning of the children.
     * @author Peter Jones
    **/
    //####################################################################
    const_iterator begin (void) const;

    //####################################################################
    /** 
     * Get an iterator that points one past the last child for this node.
     *
     * @return A "one past the end" iterator.
     * @author Peter Jones
    **/
    //####################################################################
    iterator end (void) { return iterator(); }

    //####################################################################
    /** 
     * Get a const_iterator that points one past the last child for this
     * node.
     *
     * @return A "one past the end" const_iterator
     * @author Peter Jones
    **/
    //####################################################################
    const_iterator end (void) const { return const_iterator(); }

    //####################################################################
    /** 
     * Get an iterator that points back at this node.
     *
     * @return An iterator that points at this node.
     * @author Peter Jones
    **/
    //####################################################################
    iterator self (void);

    //####################################################################
    /** 
     * Get a const_iterator that points back at this node.
     *
     * @return A const_iterator that points at this node.
     * @author Peter Jones
    **/
    //####################################################################
    const_iterator self (void) const;

    //####################################################################
    /** 
     * Get an iterator that points at the parent of this node. If this node
     * does not have a parent, this member function will return an "end"
     * iterator.
     *
     * @return An iterator that points to this nodes parent.
     * @return If no parent, returns the same iterator that xml::node::end() returns.
     * @author Peter Jones
    **/
    //####################################################################
    iterator parent (void);

    //####################################################################
    /** 
     * Get a const_iterator that points at the parent of this node. If this
     * node does not have a parent, this member function will return an
     * "end" const_iterator.
     *
     * @return A const_iterator that points to this nodes parent.
     * @return If no parent, returns the same const_iterator that xml::node::end() returns.
     * @author Peter Jones
    **/
    //####################################################################
    const_iterator parent (void) const;

    //####################################################################
    /** 
     * Find the first child node that has the given name. If no such node
     * can be found, this function will return the same iterator that end()
     * would return.
     *
     * This function is not recursive. That is, it will not search down the
     * tree for the requested node. Instead, it will only search one level
     * deep, only checking the children of this node.
     *
     * @param name The name of the node you want to find.
     * @return An iterator that points to the node if found.
     * @return An end() iterator if the node was not found.
     * @author Peter Jones
     *
     * @see elements(const char*), find(const char*, iterator)
    **/
    //####################################################################
    iterator find (const char *name);

    //####################################################################
    /** 
     * Find the first child node that has the given name. If no such node
     * can be found, this function will return the same const_iterator that
     * end() would return.
     *
     * This function is not recursive. That is, it will not search down the
     * tree for the requested node. Instead, it will only search one level
     * deep, only checking the children of this node.
     *
     * @param name The name of the node you want to find.
     * @return A const_iterator that points to the node if found.
     * @return An end() const_iterator if the node was not found.
     * @author Peter Jones
     *
     * @see elements(const char*) const, find(const char*, const_iterator) const
    **/
    //####################################################################
    const_iterator find (const char *name) const;

    //####################################################################
    /** 
     * Find the first child node, starting with the given iterator, that has
     * the given name. If no such node can be found, this function will
     * return the same iterator that end() would return.
     *
     * This function should be given an iterator to one of this node's
     * children. The search will begin with that node and continue with all
     * its sibliings. This function will not recurse down the tree, it only
     * searches in one level.
     *
     * @param name The name of the node you want to find.
     * @param start Where to begin the search.
     * @return An iterator that points to the node if found.
     * @return An end() iterator if the node was not found.
     * @author Peter Jones
     *
     * @see elements(const char*)
    **/
    //####################################################################
    iterator find (const char *name, const iterator& start);

    //####################################################################
    /** 
     * Find the first child node, starting with the given const_iterator,
     * that has the given name. If no such node can be found, this function
     * will return the same const_iterator that end() would return.
     *
     * This function should be given a const_iterator to one of this node's
     * children. The search will begin with that node and continue with all
     * its siblings. This function will not recurse down the tree, it only
     * searches in one level.
     *
     * @param name The name of the node you want to find.
     * @param start Where to begin the search.
     * @return A const_iterator that points to the node if found.
     * @return An end() const_iterator if the node was not found.
     * @author Peter Jones
     *
     * @see elements(const char*) const
    **/
    //####################################################################
    const_iterator find (const char *name, const const_iterator& start) const;

    /**
     * Returns view of child nodes of type type_element. If no such node
     * can be found, returns empty view.
     *
     * Example:
     * @code
     * xml::nodes_view view(root.elements());
     * for (xml::nodes_view::iterator i = view.begin(); i != view.end(); ++i)
     * {
     *   ...
     * }
     * @endcode
     *
     * @return View with all child elements or empty view if there aren't any.
     * @author Vaclav Slavik
     * @since  0.6.0
     *
     * @see nodes_view
    **/
    nodes_view elements();

    /**
     * Returns view of child nodes of type type_element. If no such node
     * can be found, returns empty view.
     *
     * Example:
     * @code
     * xml::const_nodes_view view(root.elements());
     * for (xml::const_nodes_view::const_iterator i = view.begin();
     *      i != view.end();
     *      ++i)
     * {
     *   ...
     * }
     * @endcode
     *
     * @return View with all child elements or empty view if there aren't any.
     * @author Vaclav Slavik
     * @since  0.6.0
     *
     * @see const_nodes_view
    **/
    const_nodes_view elements() const;

    /**
     * Returns view of child nodes of type type_element with name @a name.
     * If no such node can be found, returns empty view.
     *
     * Example:
     * @code
     * xml::nodes_view persons(root.elements("person"));
     * for (xml::nodes_view::iterator i = view.begin(); i != view.end(); ++i)
     * {
     *   ...
     * }
     * @endcode
     *
     * @param  name Name of the elements to return.
     * @return View that contains only elements @a name.
     * @author Vaclav Slavik
     * @since  0.6.0
    **/
    nodes_view elements(const char *name);

    /**
     * Returns view of child nodes of type type_element with name @a name.
     * If no such node can be found, returns empty view.
     *
     * Example:
     * @code
     * xml::const_nodes_view persons(root.elements("person"));
     * for (xml::const_nodes_view::const_iterator i = view.begin();
     *      i != view.end();
     *      ++i)
     * {
     *   ...
     * }
     * @endcode
     *
     * @param  name Name of the elements to return.
     * @return View that contains only elements @a name.
     * @author Vaclav Slavik
     * @since  0.6.0
    **/
    const_nodes_view elements(const char *name) const;

    //####################################################################
    /** 
     * Insert a new child node. The new node will be inserted at the end of
     * the child list. This is similar to the xml::node::push_back member
     * function except that an iterator to the inserted node is returned.
     *
     * @param n The node to insert as a child of this node.
     * @return An iterator that points to the newly inserted node.
     * @author Peter Jones
    **/
    //####################################################################
    iterator insert (const node &n);

    //####################################################################
    /** 
     * Insert a new child node. The new node will be inserted before the
     * node pointed to by the given iterator.
     *
     * @param position An iterator that points to the location where the new node should be inserted (before it).
     * @param n The node to insert as a child of this node.
     * @return An iterator that points to the newly inserted node.
     * @author Peter Jones
    **/
    //####################################################################
    iterator insert (const iterator& position, const node &n);

    //####################################################################
    /** 
     * Replace the node pointed to by the given iterator with another node.
     * The old node will be removed, including all its children, and
     * replaced with the new node. This will invalidate any iterators that
     * point to the node to be replaced, or any pointers or references to
     * that node.
     *
     * @param old_node An iterator that points to the node that should be removed.
     * @param new_node The node to put in old_node's place.
     * @return An iterator that points to the new node.
     * @author Peter Jones
    **/
    //####################################################################
    iterator replace (const iterator& old_node, const node &new_node);

    //####################################################################
    /** 
     * Erase the node that is pointed to by the given iterator. The node
     * and all its children will be removed from this node. This will
     * invalidate any iterators that point to the node to be erased, or any
     * pointers or references to that node.
     *
     * @param to_erase An iterator that points to the node to be erased.
     * @return An iterator that points to the node after the one being erased.
     * @author Peter Jones
     * @author Gary A. Passero
    **/
    //####################################################################
    iterator erase (const iterator& to_erase);

    //####################################################################
    /** 
     * Erase all nodes in the given range, from frist to last. This will
     * invalidate any iterators that point to the nodes to be erased, or any
     * pointers or references to those nodes.
     * 
     * @param first The first node in the range to be removed.
     * @param last An iterator that points one past the last node to erase. Think xml::node::end().
     * @return An iterator that points to the node after the last one being erased.
     * @author Peter Jones
    **/
    //####################################################################
    iterator erase (iterator first, const iterator& last);

    //####################################################################
    /** 
     * Erase all children nodes with the given name. This will find all
     * nodes that have the given node name and remove them from this node.
     * This will invalidate any iterators that point to the nodes to be
     * erased, or any pointers or references to those nodes.
     *
     * @param name The name of nodes to remove.
     * @return The number of nodes removed.
     * @author Peter Jones
    **/
    //####################################################################
    size_type erase (const char *name);

    //####################################################################
    /** 
     * Sort all the children nodes of this node using one of thier
     * attributes. Only nodes that are of xml::node::type_element will be
     * sorted, and they must have the given node_name.
     *
     * The sorting is done by calling std::strcmp on the value of the given
     * attribute.
     *
     * @param node_name The name of the nodes to sort.
     * @param attr_name The attribute to sort on.
     * @author Peter Jones
    **/
    //####################################################################
    void sort (const char *node_name, const char *attr_name);

    //####################################################################
    /** 
     * Sort all the children nodes of this node using the given comparison
     * function object. All element type nodes will be considered for
     * sorting.
     *
     * @param compare The binary function object to call in order to sort all child nodes.
     * @author Peter Jones
    **/
    //####################################################################
    template <typename T> void sort (T compare)
    { impl::sort_callback<T> cb(compare); sort_fo(cb); }

    //####################################################################
    /** 
     * Convert the node and all its children into XML text and set the given
     * string to that text.
     *
     * @param xml The string to set the node's XML data to.
     * @author Peter Jones
    **/
    //####################################################################
    void node_to_string (std::string &xml) const;

    //####################################################################
    /** 
     * Write a node and all of its children to the given stream.
     *
     * @param stream The stream to write the node as XML.
     * @param n The node to write to the stream.
     * @return The stream.
     * @author Peter Jones
    **/
    //####################################################################
    friend std::ostream& operator<< (std::ostream &stream, const node &n);

private:
    impl::node_impl *pimpl_;

    // private ctor to create uninitialized instance
    explicit node (int);

    void set_node_data (void *data);
    void* get_node_data (void);
    void* release_node_data (void);
    friend class tree_parser;
    friend class impl::node_iterator;
    friend class document;
    friend struct impl::doc_impl;
    friend struct impl::node_cmp;

    void sort_fo (impl::cbfo_node_compare &fo);

    xml::ns add_namespace_def (const char *uri, const char *prefix);
    xml::ns add_matched_namespace_def (void *libxml2RawNamespace, const char *uri, ns_definition_adding_type type);
}; // end xml::node class

} // end xml namespace
#endif
