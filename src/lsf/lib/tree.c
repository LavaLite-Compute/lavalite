/*
 * Copyright (C) 2014-2015 David Bigagli
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA
 *
 */

#include "tree.h"
#include "link.h"

/* tree_init()
 */
struct tree_ *tree_init(const char *name)
{
    struct tree_ *t;
    int l;

    t = calloc(1, sizeof(struct tree_));
    assert(t);

    /* /name
     */
    l = strlen(name) + 2;
    t->name = calloc(l, sizeof(char));
    sprintf(t->name, "/%s", name);

    t->root = calloc(1, sizeof(struct tree_node_));
    assert(t->root);
    t->root->name = strdup("/");
    t->root->parent = NULL;
    t->node_tab = hash_make(11);
    t->leafs = make_link();

    return t;
}

/* tree_insert_node()
 *
 * Insert child as leftmost respect to the parent
 *
 * Before:
 * r->e
 *
 * After
 *
 * r->child->e
 */
struct tree_node_ *tree_insert_node(struct tree_node_ *parent,
                                    struct tree_node_ *child)
{
    if (parent == NULL)
        return NULL;

    /* No children yet..
     */
    if (parent->child == NULL) {
        parent->child = child;
        child->parent = parent;

        return child;
    }

    /* Insert as left most
     */
    child->parent = parent;
    child->right = parent->child;
    child->left = parent->child->left;
    parent->child->left = child;
    parent->child = child;

    return child;
}

/* tree_rm_node()
 */
struct tree_node_ *tree_rm_node(struct tree_node_ *n)
{
    if (n->right)
        n->right->left = n->left;
    if (n->left)
        n->left->right = n->right;
    if (n->parent && n->parent->child == n)
        n->parent->child = n->right;
    return n;
}

/* tree_print()
 *
 * Print using stack traversal.
 *
 */
int tree_print(struct tree_node_ *n, struct tree_ *t)
{
    if (n == NULL) {
        printf("%s: tree traversal done\n", __func__);
        return 0;
    }

    printf("%s: node name %s\n", __func__, n->name);

    return 0;
}

/* tree_walk()
 *
 * Walk on the tree from the root down
 * level by level and for each node run
 * the function f(). Tree operator...
 * In other words, traverse the link of
 * siblings then dive down to the next
 * level. Level order.
 */
int tree_walk(struct tree_ *r, int (*f)(struct tree_node_ *, struct tree_ *))
{
    link_t *link;
    struct tree_node_ *n;
    int cc;

    /* funny tree, no nodes...?
     */
    if (!r->root)
        return -1;

    n = r->root;

    /* All right only one node...
     */
    if (!n->child) {
        /* protocol, (*f)() may want
         * to know when we are done.
         */
        return (*f)(NULL, NULL);
    }

    /* Init my stack...
     */
    link = make_link();

Loop:
    while (n) {
        /* traverse all siblings and enqueue (not push)
         * on the stack subroots...
         */
        if (n->child)
            enqueue_link(link, n);

        cc = (*f)(n, r);
        /* protocol, the caller is asking
         * me to stop the traversal, sure...
         */
        if (cc < 0) {
            fin_link(link);
            return cc;
        }
        n = n->right;
    }

    /* pop each subroot and traverse
     * again...
     */
    n = pop_link(link);
    if (n) {
        n = n->child;
        goto Loop; /* don't you love this */
    }

    fin_link(link);
    /* a little protocol... tell
     * him we are done with the
     * traversal.
     */

    return (*f)(NULL, NULL);
}

/* tree_next_node()
 *
 * Run in while() loop
 */
struct tree_node_ *tree_next_node(struct tree_node_ *n)
{
    struct tree_node_ *parent;

    if (n == NULL)
        return NULL;

    if (n->child)
        return n->child;
    if (n->right)
        return n->right;

    parent = n->parent;
    while (parent) {
        if (parent->right)
            return parent->right;
        parent = parent->parent;
    }
    return NULL;
}

/* tree_walk2()
 *
 * Visit the root first than each subtree.
 * Gives the same output as tree_next_node()
 */
int tree_walk2(struct tree_ *t, int (*f)(struct tree_node_ *, struct tree_ *))
{
    link_t *link;
    struct tree_node_ *n;

    if (t->root == NULL)
        return -1;

    link = make_link();
    n = t->root;

znovu:
    while (n) {
        (*f)(n, t);

        if (n->child && n->right) {
            push_link(link, n);
        }

        if (n->child) {
            n = n->child;
            continue;
        }

        n = n->right;
    }

    n = pop_link(link);
    if (n) {
        n = n->right;
        goto znovu;
    }

    fin_link(link);
    return (*f)(NULL, NULL);
}

/* tree_rm_leaf()
 *
 * Walk down till you find a child and remove it
 */
struct tree_node_ *tree_rm_leaf(struct tree_node_ *n)
{
    if (n == NULL)
        return NULL;

    while (n) {
        if (n->child)
            n = n->child;
        else
            break;
    }

    tree_rm_node(n);

    return n;
}

/* tree_free()
 */
void tree_free(struct tree_ *t)
{
    struct tree_node_ *n;

    while ((n = tree_rm_leaf(t->root))) {
        if (n == t->root)
            break;
        _free_(n->name);
        _free_(n);
    }

    fin_link(t->leafs);
    _free_(t->root->name);
    _free_(t->root);
    _free_(t->name);
    hash_free(t->node_tab, NULL);
    _free_(t);
}
