/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de>,
              Andreas Hundt <andi@convergence.de> and 
              Sven Neumann <sven@convergence.de>

   Balanced binary tree ported from glib-2.0.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#include <stdlib.h>

#include "tree.h"


typedef struct _Node Node;

struct _Tree
{
     Node *root;
};

struct _Node
{
     int balance;
     Node *left;
     Node *right;
     void *key;
     void *value;
};

static Node * tree_node_new          (void  *key,
                                      void  *value);
static void   tree_node_destroy      (Node  *node);
static Node * tree_node_insert       (Tree  *tree,
                                      Node  *node,
                                      void  *key,
                                      void  *value,
                                      int   *inserted);
static Node * tree_node_lookup       (Node  *node, 
                                      void  *key);
static Node * tree_node_balance      (Node  *node);
static Node * tree_node_rotate_left  (Node  *node);
static Node * tree_node_rotate_right (Node  *node);


Tree * tree_new (void)
{
     Tree *tree;

     tree = malloc (sizeof (Tree));
     if (tree)
          tree->root = NULL;
  
     return tree;
}

void tree_destroy (Tree *tree)
{
     if (tree) {
          tree_node_destroy (tree->root);
          free (tree);
     }
}

void tree_insert (Tree *tree,
                  void *key,
                  void *value)
{
     int inserted = 0;

     tree->root = tree_node_insert (tree,
                                    tree->root,
                                    key, value, 
                                    &inserted);
}

void * tree_lookup (Tree *tree,
                    void *key)
{
     Node *node;

     node = tree_node_lookup (tree->root, key);

     return (node ? node->value : NULL);
}

static Node *
tree_node_new (void *key,
               void *value)
{
     Node *node;

     node = malloc (sizeof (Node));

     node->balance = 0;
     node->left    = NULL;
     node->right   = NULL;
     node->key     = key;
     node->value   = value;

     return node;
}

static void
tree_node_destroy (Node *node)
{
     if (node) {
          tree_node_destroy (node->left);
          tree_node_destroy (node->right);

          if (node->value)
               free (node->value);
          free (node);
     }
}

static Node *
tree_node_insert (Tree *tree,
                  Node *node,
                  void *key,
                  void *value,
                  int  *inserted)
{
     int cmp;
     int old_balance;

     if (!node) {
          *inserted = 1;
          return tree_node_new (key, value);
     }
  
     cmp = key - node->key;
     if (cmp == 0) {
          node->value = value;
          return node;
     }
  
     if (cmp < 0) {
          if (node->left) {
               old_balance = node->left->balance;
	       node->left = tree_node_insert (tree, node->left,
                                              key, value, inserted);

               if ((old_balance != node->left->balance) && node->left->balance)
                   node->balance -= 1;
          }
          else {
               *inserted = 1;
               node->left = tree_node_new (key, value);
               node->balance -= 1;
          }
     }
     else if (cmp > 0) {
          if (node->right) {
               old_balance = node->right->balance;
               node->right = tree_node_insert (tree, node->right,
                                               key, value, inserted);
               
               if ((old_balance != node->right->balance) && node->right->balance)
                    node->balance += 1;
          }
          else {
               *inserted = 1;
               node->right = tree_node_new (key, value);
               node->balance += 1;
          }
     }

     if (*inserted && (node->balance < -1 || node->balance > 1))
          node = tree_node_balance (node);

     return node;
}

static Node * tree_node_lookup (Node *node, 
                                void *key)
{
     int cmp;

     if (!node)
          return NULL;

     cmp = key - node->key;
     if (cmp == 0)
          return node;

     if (cmp < 0 && node->left) {
          return tree_node_lookup (node->left, key);
     }
     else if (cmp > 0 && node->right) {
          return tree_node_lookup (node->right, key);
     }

     return NULL;
}

static Node * tree_node_balance (Node *node)
{
     if (node->balance < -1) {
          if (node->left->balance > 0)
               node->left = tree_node_rotate_left (node->left);
          node = tree_node_rotate_right (node);
     }
     else if (node->balance > 1) {
          if (node->right->balance < 0)
               node->right = tree_node_rotate_right (node->right);
          node = tree_node_rotate_left (node);
     }

     return node;
}

static Node * tree_node_rotate_left (Node *node)
{
     Node *right;
     int   a_bal;
     int   b_bal;

     right = node->right;

     node->right = right->left;
     right->left = node;

     a_bal = node->balance;
     b_bal = right->balance;

     if (b_bal <= 0) {
          if (a_bal >= 1)
               right->balance = b_bal - 1;
          else
               right->balance = a_bal + b_bal - 2;
          node->balance = a_bal - 1;
     }
     else {
          if (a_bal <= b_bal)
               right->balance = a_bal - 2;
          else
               right->balance = b_bal - 1;
          node->balance = a_bal - b_bal - 1;
     }

     return right;
}

static Node * tree_node_rotate_right (Node *node)
{
     Node *left;
     int   a_bal;
     int   b_bal;

     left = node->left;

     node->left = left->right;
     left->right = node;

     a_bal = node->balance;
     b_bal = left->balance;

     if (b_bal <= 0) {
          if (b_bal > a_bal)
               left->balance = b_bal + 1;
          else
               left->balance = a_bal + 2;
          node->balance = a_bal - b_bal + 1;
     }
     else {
          if (a_bal <= -1)
               left->balance = b_bal + 1;
          else
               left->balance = a_bal + b_bal + 2;
          node->balance = a_bal + 1;
     }

     return left;
}
