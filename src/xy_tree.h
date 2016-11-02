#ifndef XY_TREE_H
#define XY_TREE_H
/*
 * -----------------------------------------------------------------
 * red-black tree taken from Emim Martinian's
 * Red-Black Tree C-Code (See http://freshmeat.net/projects/rbtree)
 *
 * modified for use with xy_lib by Jochen Karrer 02/05/2002
 * -----------------------------------------------------------------
 */

#include<stdio.h>
#include<stdlib.h>

/* --------------------------------------------------------------------------*/
/*  CONVENTIONS:  All data structures for red-black trees have the prefix */
/*                "rb_" to prevent name conflicts. */
/*                                                                      */
/*                Function names: Each word in a function name begins with */
/*                a capital letter.  An example funcntion name is  */
/*                CreateRedTree(a,b,c). Furthermore, each function name */
/*                should begin with a capital letter to easily distinguish */
/*                them from variables. */
/*                                                                     */
/*                Variable names: Each word in a variable name begins with */
/*                a capital letter EXCEPT the first letter of the variable */
/*                name.  For example, int newLongInt.  Global variables have */
/*                names beginning with "g".  An example of a global */
/*                variable name is gNewtonsConstant. */
/* --------------------------------------------------------------------------*/

typedef struct xy_node {
	// all fields are private;
	void *key;
	void *value;
	int red;		/* if red=0 then the node is black */
	struct xy_node *left;
	struct xy_node *right;
	struct xy_node *parent;
} xy_node;

/* Compare(a,b) should return 1 if *a > *b, -1 if *a < *b, and 0 otherwise */
/* Destroy(a) takes a pointer to whatever key might be and frees it accordingly */
typedef struct XY_Tree {
	// all fields are private
	int (*Compare) (const void *a, const void *b);
	void (*DestroyKey) (void *a);
	void (*DestroyValue) (void *a);
	void (*DestroyNode) (void *a);
	/*  A sentinel is used for root and for nil.  These sentinels are */
	/*  created when RBTreeCreate is called.  root->left should always */
	/*  point to the node which is the root of the tree.  nilp points to a */
	/*  node which should always be black but has aribtrary children and */
	/*  parent and no key or info.  The point of using these sentinels is so */
	/*  that the root and nil nodes do not require special cases in the code */
	xy_node root;
	xy_node *rootp;
	xy_node nil;
	xy_node *nilp;
} XY_Tree;

void XY_InitTree(struct XY_Tree *tree, int (*CompFunc) (const void *, const void *), void (*DestFunc) (void *), void (*ValueDestFunc) (void *), void (*nodeDestFunc) (void *)	//  has to point to free() if CreateTreeNode is used
    );
xy_node *XY_CreateTreeNode(struct XY_Tree *, void *key, void *val);
void XY_AddTreeNode(struct XY_Tree *, xy_node * node, void *key, void *val);	// no malloc of node
void XY_DeleteTreeNode(struct XY_Tree *, xy_node *);

void XY_DeleteTree(struct XY_Tree *);
xy_node *XY_FindTreeNode(struct XY_Tree *, void *);
xy_node *XY_FindNextTreeNode(struct XY_Tree *, void *);

xy_node *XY_FirstTreeNode(struct XY_Tree *);
xy_node *XY_NextTreeNode(struct XY_Tree *, xy_node * node);
xy_node *XY_FindLeftTreeNode(XY_Tree * tree, void *);
#define XY_NodeValue(node) ((node)->value)
#define XY_NodeKey(node) ((node)->key)
#endif				// XY_TREE_H
