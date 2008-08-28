/* EINA - EFL data type library
 * Copyright (C) 2008 Cedric Bail
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library;
 * if not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "eina_rbtree.h"
#include "eina_array.h"
#include "eina_private.h"

/*============================================================================*
 *                                  Local                                     *
 *============================================================================*/

#define EINA_RBTREE_ITERATOR_PREFIX_MASK  0x1
#define EINA_RBTREE_ITERATOR_INFIX_MASK   0x2
#define EINA_RBTREE_ITERATOR_POSTFIX_MASK 0x4

typedef struct _Eina_Iterator_Rbtree Eina_Iterator_Rbtree;
typedef struct _Eina_Iterator_Rbtree_List Eina_Iterator_Rbtree_List;

struct _Eina_Iterator_Rbtree
{
   Eina_Iterator iterator;

   Eina_Array *stack;

   unsigned char mask;
};

struct _Eina_Iterator_Rbtree_List
{
   Eina_Rbtree *tree;

   Eina_Rbtree_Direction dir : 1;
   Eina_Bool up : 1;
};

static Eina_Iterator_Rbtree_List *
_eina_rbtree_iterator_list_new(const Eina_Rbtree *tree)
{
   Eina_Iterator_Rbtree_List *new;

   new = malloc(sizeof (Eina_Iterator_Rbtree_List));
   if (!new) return NULL;

   new->tree = (Eina_Rbtree*) tree;
   new->dir = EINA_RBTREE_RIGHT;
   new->up = EINA_FALSE;

   return new;
}

static Eina_Rbtree *
_eina_rbtree_iterator_get_content(Eina_Iterator_Rbtree *it)
{
   if (eina_array_count(it->stack) <= 0) return NULL;
   return eina_array_get(it->stack, eina_array_count(it->stack) - 1);
}

static void
_eina_rbtree_iterator_free(Eina_Iterator_Rbtree *it)
{
   Eina_Iterator_Rbtree_List *item;
   Eina_Array_Iterator et;
   unsigned int i;

   EINA_ARRAY_ITER_NEXT(it->stack, i, item, et)
     free(item);

   eina_array_free(it->stack);
   free(it);
}

static Eina_Bool
_eina_rbtree_iterator_next(Eina_Iterator_Rbtree *it, void **data)
{
   Eina_Iterator_Rbtree_List *last;
   Eina_Iterator_Rbtree_List *new;
   Eina_Rbtree *tree;

   if (eina_array_count(it->stack) <= 0) return EINA_FALSE;

   last = eina_array_get(it->stack, eina_array_count(it->stack) - 1);
   tree = last->tree;

   if (last->tree == NULL || last->up == EINA_TRUE)
     {
	last = eina_array_pop(it->stack);
	while (last->dir == EINA_RBTREE_LEFT
	       || last->tree == NULL)
	  {
	     if (tree)
	       if ((it->mask & EINA_RBTREE_ITERATOR_POSTFIX_MASK) == EINA_RBTREE_ITERATOR_POSTFIX_MASK)
		 {
		    free(last);

		    if (eina_array_count(it->stack) > 0)
		      {
			 last = eina_array_get(it->stack, eina_array_count(it->stack) - 1);
			 last->up = EINA_TRUE;
		      }

		    goto onfix;
		 }

	     free(last);

	     last = eina_array_pop(it->stack);
	     if (!last) return EINA_FALSE;
	     tree = last->tree;
	  }

	last->dir = EINA_RBTREE_LEFT;
	last->up = EINA_FALSE;

	eina_array_push(it->stack, last);

	if ((it->mask & EINA_RBTREE_ITERATOR_INFIX_MASK) == EINA_RBTREE_ITERATOR_INFIX_MASK)
	  goto onfix;
     }

   new = _eina_rbtree_iterator_list_new(last->tree->son[last->dir]);
   if (!new) return EINA_FALSE;
   eina_array_push(it->stack, new);

   if (last->dir == EINA_RBTREE_RIGHT)
     if ((it->mask & EINA_RBTREE_ITERATOR_PREFIX_MASK) == EINA_RBTREE_ITERATOR_PREFIX_MASK)
       goto onfix;

   return _eina_rbtree_iterator_next(it, data);

 onfix:
   if (data) *data = tree;
   return EINA_TRUE;
}

static Eina_Iterator *
_eina_rbtree_iterator_build(const Eina_Rbtree *root, unsigned char mask)
{
   Eina_Iterator_Rbtree_List *first;
   Eina_Iterator_Rbtree *it;

   if (!root) return NULL;

   it = calloc(1, sizeof (Eina_Iterator_Rbtree));
   if (!it) return NULL;

   it->stack = eina_array_new(8);
   if (!it->stack) goto on_error;

   first = _eina_rbtree_iterator_list_new(root);
   if (!first) goto on_error;
   eina_array_push(it->stack, first);

   it->mask = mask;

   it->iterator.next = FUNC_ITERATOR_NEXT(_eina_rbtree_iterator_next);
   it->iterator.get_container = FUNC_ITERATOR_GET_CONTAINER(_eina_rbtree_iterator_get_content);
   it->iterator.free = FUNC_ITERATOR_FREE(_eina_rbtree_iterator_free);

   return &it->iterator;

 on_error:
   if (it && it->stack) eina_array_free(it->stack);
   if (it) free(it);

   return NULL;
}

/*
 * Thanks to Julienne Walker public domain tutorial.
 * http://eternallyconfuzzled.com/tuts/datastructures/jsw_tut_rbtree.aspx
 */

static void
_eina_rbtree_node_init(Eina_Rbtree *node)
{
   if (!node) return ;

   node->son[0] = NULL;
   node->son[1] = NULL;

   node->color = EINA_RBTREE_RED;
}

static inline Eina_Bool
_eina_rbtree_is_red(Eina_Rbtree *node)
{
   return node != NULL && node->color == EINA_RBTREE_RED;
}

static inline Eina_Rbtree *
_eina_rbtree_inline_single_rotation(Eina_Rbtree *node, Eina_Rbtree_Direction dir)
{
   Eina_Rbtree *save = node->son[!dir];

   node->son[!dir] = save->son[dir];
   save->son[dir] = node;

   node->color = EINA_RBTREE_RED;
   save->color = EINA_RBTREE_BLACK;

   return save;
}

static inline Eina_Rbtree *
_eina_rbtree_inline_double_rotation(Eina_Rbtree *node, Eina_Rbtree_Direction dir)
{
   node->son[!dir] = _eina_rbtree_inline_single_rotation(node->son[!dir], !dir);
   return _eina_rbtree_inline_single_rotation(node, dir);
}

/*============================================================================*
 *                                 Global                                     *
 *============================================================================*/

/*============================================================================*
 *                                   API                                      *
 *============================================================================*/

EAPI Eina_Rbtree *
eina_rbtree_inline_insert(Eina_Rbtree *root, Eina_Rbtree *node, Eina_Rbtree_Cmp_Node_Cb cmp)
{
   Eina_Rbtree head;
   Eina_Rbtree *g, *t;  /* Grandparent & parent */
   Eina_Rbtree *p, *q;  /* Iterator & parent */
   Eina_Rbtree_Direction dir, last;

   if (!node) return root;

   _eina_rbtree_node_init(node);

   if (!root)
     {
	root = node;
	goto end_add;
     }

   memset(&head, 0, sizeof (Eina_Rbtree));
   dir = EINA_RBTREE_LEFT;

   /* Set up helpers */
   t = &head;
   g = p = NULL;
   q = t->son[1] = root;

   /* Search down the tree */
   for (;;)
     {
	if (q == NULL)
	  {
	     /* Insert new node at the bottom */
	     p->son[dir] = q = node;
	  }
	else if (_eina_rbtree_is_red(q->son[0])
		 && _eina_rbtree_is_red(q->son[1]))
	  {
	     /* Color flip */
	     q->color = EINA_RBTREE_RED;
	     q->son[0]->color = EINA_RBTREE_BLACK;
	     q->son[1]->color = EINA_RBTREE_BLACK;
	  }

	/* Fix red violation */
	if (_eina_rbtree_is_red(q) && _eina_rbtree_is_red(p))
	  {
	     Eina_Rbtree_Direction dir2;

	     dir2 = (t->son[1] == g) ? EINA_RBTREE_RIGHT : EINA_RBTREE_LEFT;

	     if (q == p->son[last])
	       t->son[dir2] = _eina_rbtree_inline_single_rotation(g, !last);
	     else
	       t->son[dir2] = _eina_rbtree_inline_double_rotation(g, !last);
	  }

	/* Stop if found */
	if (q == node)
	  break;

	last = dir;
	dir = cmp(q, node);

	/* Update helpers */
	if ( g != NULL )
	  t = g;
	g = p, p = q;
	q = q->son[dir];
     }

   root = head.son[1];

 end_add:
   /* Make root black */
   root->color = EINA_RBTREE_BLACK;

   return root;
}

EAPI Eina_Rbtree *
eina_rbtree_inline_remove(Eina_Rbtree *root, Eina_Rbtree *node, Eina_Rbtree_Cmp_Node_Cb cmp)
{
   Eina_Rbtree head;
   Eina_Rbtree *q, *p, *g;
   Eina_Rbtree *f = NULL;
   Eina_Rbtree_Direction dir;

   if (!root || !node) return root;

   memset(&head, 0, sizeof(Eina_Rbtree));

   dir = EINA_RBTREE_RIGHT;
   q = &head;
   g = p = NULL;
   q->son[EINA_RBTREE_RIGHT] = root;

   /* Search and push a red down */
   while (q->son[dir] != NULL)
     {
	Eina_Rbtree_Direction last = dir;

	/* Update helpers */
	g = p; p = q;
	q = q->son[dir];
	dir = cmp(q, node);

	/* Save parent node found */
	if (q == node)
	  f = p;

	/* Push the red node down */
	if (!_eina_rbtree_is_red(q)
	    && !_eina_rbtree_is_red(q->son[dir]))
	  {
	     if (_eina_rbtree_is_red(q->son[!dir]))
	       q = p->son[last] = _eina_rbtree_inline_single_rotation(q, dir);
	     else if (!_eina_rbtree_is_red(q->son[!dir])) {
		Eina_Rbtree *s = p->son[!last];

		if (s != NULL)
		  {
		     if (!_eina_rbtree_is_red(s->son[!last])
			 && !_eina_rbtree_is_red(s->son[last]))
		       {
			  /* Color flip */
			  p->color = EINA_RBTREE_BLACK;
			  p->son[0]->color = EINA_RBTREE_RED;
			  p->son[1]->color = EINA_RBTREE_RED;
		       }
		     else
		       {
			  Eina_Rbtree_Direction dir2;

			  dir2 = g->son[1] == p ? EINA_RBTREE_RIGHT : EINA_RBTREE_LEFT;

			  if (_eina_rbtree_is_red(s->son[last]))
			    g->son[dir2] = _eina_rbtree_inline_double_rotation(g->son[dir2], last);
			  else if (_eina_rbtree_is_red(s->son[!last]))
			    g->son[dir2] = _eina_rbtree_inline_single_rotation(g->son[dir2], last);

			  /* Ensure correct coloring */
			  q->color = g->son[dir2]->color = EINA_RBTREE_RED;
			  g->son[dir2]->son[EINA_RBTREE_LEFT]->color = EINA_RBTREE_BLACK;
			  g->son[dir2]->son[EINA_RBTREE_RIGHT]->color = EINA_RBTREE_BLACK;
		       }
		  }
	     }
	  }
     }

   /* Replace and remove if found */
   if (f != NULL)
     {
	/* 'q' should take the place of 'node' parent */
	f->son[f->son[1] == node] = q;

	/* Switch the link from the parent to q's son */
	p->son[p->son[1] == q] = q->son[q->son[0] == NULL];

	/* Put q at the place of node */
	q->son[0] = node->son[0];
	q->son[1] = node->son[1];
	q->color = node->color;

	/* Reset node link */
	node->son[0] = NULL;
	node->son[1] = NULL;
     }

   root = head.son[1];
   if (root != NULL)
     root->color = EINA_RBTREE_BLACK;

   return root;
}

EAPI Eina_Rbtree *
eina_rbtree_inline_lookup(Eina_Rbtree *root, const void *key, int length, Eina_Rbtree_Cmp_Key_Cb cmp)
{
   int result;

   while (root)
     {
	result = cmp(root, key, length);
	if (result == 0) return root;

	root = root->son[result < 0 ? 0 : 1];
     }

   return NULL;
}

EAPI Eina_Iterator *
eina_rbtree_iterator_prefix(const Eina_Rbtree *root)
{
   return _eina_rbtree_iterator_build(root, EINA_RBTREE_ITERATOR_PREFIX_MASK);
}

EAPI Eina_Iterator *
eina_rbtree_iterator_infix(const Eina_Rbtree *root)
{
   return _eina_rbtree_iterator_build(root, EINA_RBTREE_ITERATOR_INFIX_MASK);
}

EAPI Eina_Iterator *
eina_rbtree_iterator_postfix(const Eina_Rbtree *root)
{
   return _eina_rbtree_iterator_build(root, EINA_RBTREE_ITERATOR_POSTFIX_MASK);
}
