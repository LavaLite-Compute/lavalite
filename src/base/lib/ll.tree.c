/* ------------------------------------------------------------------------
 * LavaLite — High-Performance Job Scheduling Infrastructure
 *
 * Copyright (C) LavaLite Contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 * ------------------------------------------------------------------------ */

#include "lsf/lib/ll_tree.h"

#include <stdlib.h>
#include <stddef.h>


// Initialize an existing tree container
void ll_tree_init(struct ll_tree *tree) {
    tree->root = NULL;
}

// Create a new tree node
struct ll_tree_node *ll_tree_node_create(int kind, void *payload) {
    struct ll_tree_node *node = malloc(sizeof(struct ll_tree_node));
    if (node == NULL) return NULL;

    node->kind = kind;
    node->payload = payload;
    node->parent = NULL;
    node->first_child = NULL;
    node->next_sibling = NULL;
    node->prev_sibling = NULL;

    return node;
}

// Free a single node
void ll_tree_node_free(struct ll_tree_node *node) {
    free(node);
}

// Free a subtree iteratively
void ll_tree_free_subtree(struct ll_tree_node *node,
                          void (*cleanup)(int kind, void *payload)) {
    if (node == NULL) return;

    struct ll_stack *stack = ll_stack_create();
    ll_stack_push(stack, node);

    while (stack != NULL && !ll_stack_is_empty(stack)) {
        struct ll_tree_node *current = ll_stack_pop(stack);

        // Push all siblings
        struct ll_tree_node *sibling = current->next_sibling;
        while (sibling != NULL) {
            ll_stack_push(stack, sibling);
            sibling = sibling->next_sibling;
        }

        // Push first child
        if (current->first_child != NULL) {
            ll_stack_push(stack, current->first_child);
        }

        // Cleanup and free the node
        if (cleanup != NULL) {
            cleanup(current->kind, current->payload);
        }
        ll_tree_node_free(current);
    }

    ll_stack_free(stack);
}

// Set the root node
void ll_tree_set_root(struct ll_tree *tree, struct ll_tree_node *root) {
    tree->root = root;
    if (root != NULL) {
        root->parent = NULL;
        root->next_sibling = NULL;
        root->prev_sibling = NULL;
    }
}

// Check if tree is empty
int ll_tree_is_empty(const struct ll_tree *tree) {
    return tree->root == NULL;
}

// Add child as last child
void ll_tree_add_child(struct ll_tree_node *parent,
                       struct ll_tree_node *child) {
    child->parent = parent;
    child->next_sibling = NULL;
    child->prev_sibling = NULL;

    if (parent->first_child == NULL) {
        parent->first_child = child;
    } else {
        struct ll_tree_node *last = parent->first_child;
        while (last->next_sibling != NULL) {
            last = last->next_sibling;
        }
        last->next_sibling = child;
        child->prev_sibling = last;
    }
}

// Add child as first child
void ll_tree_add_child_front(struct ll_tree_node *parent,
                             struct ll_tree_node *child) {
    child->parent = parent;
    child->next_sibling = parent->first_child;
    child->prev_sibling = NULL;

    if (parent->first_child != NULL) {
        parent->first_child->prev_sibling = child;
    }
    parent->first_child = child;
}

// Detach node from parent and siblings
void ll_tree_detach(struct ll_tree_node *node) {
    if (node->parent != NULL) {
        // If this is the first child, update parent's pointer
        if (node->parent->first_child == node) {
            node->parent->first_child = node->next_sibling;
        }
        node->parent = NULL;
    }

    // Unlink from sibling chain
    if (node->prev_sibling != NULL) {
        node->prev_sibling->next_sibling = node->next_sibling;
    }
    if (node->next_sibling != NULL) {
        node->next_sibling->prev_sibling = node->prev_sibling;
    }

    node->prev_sibling = NULL;
    node->next_sibling = NULL;
}

// Check if node is a leaf
int ll_tree_node_is_leaf(const struct ll_tree_node *node) {
    return node->first_child == NULL;
}

// Check if node is root
int ll_tree_node_is_root(const struct ll_tree *tree,
                         const struct ll_tree_node *node) {
    return tree->root == node;
}

// Iterate over direct children
void ll_tree_for_each_child(struct ll_tree_node *parent,
                            void (*fn)(struct ll_tree_node *)) {
    struct ll_tree_node *child = parent->first_child;
    while (child != NULL) {
        fn(child);
        child = child->next_sibling;
    }
}

// Walk sibling chain
void ll_tree_walk_siblings(struct ll_tree_node *node,
                           void (*fn)(struct ll_tree_node *node)) {
    while (node != NULL) {
        fn(node);
        node = node->next_sibling;
    }
}


// Level-style traversal. This is breadth-first within each sibling group,
// then depth-first overall.
/* Traversal order for:
 *
 *       Root
 *      / |                                     \
 *     A  B  C
 *    /|    |
 *   D E    F
 * Visit order: Root → A → B → C → D → E → F
 */
void ll_tree_traverse_levels(struct ll_tree_node *root,
                             void (*fn)(struct ll_tree_node *node)) {
    if (root == NULL) return;

    struct ll_stack *stack = ll_stack_create();
    struct ll_tree_node *node = root;

process_level:
    while (node != NULL) {
        // If this node has children, save it for later descent
        if (node->first_child != NULL) {
            ll_stack_push(stack, node);
        }

        // Process current node
        fn(node);

        // Move to next sibling
        node = node->next_sibling;
    }

    // Pop parent and descend to its children
    node = ll_stack_pop(stack);
    if (node != NULL) {
        node = node->first_child;
        goto process_level;
    }

    ll_stack_free(stack);
}

// Depth-first traversal using stack
// Root → A → D → E → B → C → F
void ll_tree_traverse_dfs(struct ll_tree_node *root,
                          void (*fn)(struct ll_tree_node *node)) {
    if (root == NULL) return;

    struct ll_stack *stack = ll_stack_create();
    ll_stack_push(stack, root);

    while (stack != NULL && !ll_stack_is_empty(stack)) {
        struct ll_tree_node *node = ll_stack_pop(stack);

        // Process current node
        fn(node);

        // Push siblings right to left (so we process left to right)
        struct ll_tree_node *sibling = node->next_sibling;
        while (sibling != NULL) {
            ll_stack_push(stack, sibling);
            sibling = sibling->next_sibling;
        }

        // Push first child (will be processed next)
        if (node->first_child != NULL) {
            ll_stack_push(stack, node->first_child);
        }
    }

    ll_stack_free(stack);
}
