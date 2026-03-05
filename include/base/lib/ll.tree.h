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

#pragma once

#include "lsf/lib/ll_sys.h"

// ---------------------------------------------------------------------------
// LavaLite general tree
// ----------------------
// Non-intrusive, rooted tree with arbitrary fan-out.
//
// Each node has:
//   parent       : pointer to the parent node (NULL for root)
//   first_child  : pointer to the first child   (down, negative Y axis)
//   next_sibling : pointer to the next sibling (right, positive X axis)
//   prev_sibling : pointer to the previous sibling (left, negative X axis)
//   kind         : user-defined type tag (e.g. enum topo_type)
//   payload      : user-supplied pointer (topology node, job group, etc.)
//
// Conceptual 2D layout:
//
//             (root)
//               |
//               v   first_child          X (siblings)
//           [ node* ]--------------------------->
//              | \
//              |  \ next_sibling
//              |   v
//              |  [ node* ] <-> [ node* ] <-> [ node* ]
//              v
//              Y (children)
//
// The tree library manages only shape and node allocation. It never frees or
// interprets the payload pointer; 'kind' is for the caller's own dispatch.
// ---------------------------------------------------------------------------

/* Practical application
 *
 * Scenario 1: Fill hosts efficiently (DFS)
 *
 * Cluster
 * ├── Partition (e.g., "gpu", "highmem", "standard")
 * │   ├── Host
 * │   │   ├── CPU Socket
 * │   │   │   ├── Core
 * │   │   │   └── Core
 * │   │   └── GPU (if present)
 * │   └── Host
 * └── Partition
 *
 * Run 1000 single-core jobs, I don't care where
 * DFS fills one host completely before moving to next (packing)
 * Better resource utilization, less fragmentation
 *
 * Scenario 2: Network-aware placement (Level traversal)
 *
 * Core Switch (or Spine Switch)
 * ├── Aggregation/Distribution Switch (ToR - Top of Rack)
 * │   ├── Rack 1
 * │   │   ├── Host (compute node)
 * │   │   └── Host
 * │   └── Rack 2
 * └── Aggregation Switch
 *
 * Give me 2 racks under the same ToR switch, each rack needs 64 cores free
 * Level traversal lets you:
 * 1. Find ToR switch
 * 2. Examine all its racks at once (siblings)
 * 3. Pick best 2 racks with locality
 *
 * Scenario 3: MPI-style job needing low-latency network
 *
 * MPI-style job needing low-latency network
 * "I need 128 cores all under the same ToR for tight communication"
 *
 * Level traversal examines all racks under one ToR before moving to next ToR
 *
 /

struct ll_tree_node {
    enum kind;       // user-defined type tag (e.g. enum topo_type)
    void *payload;  // user object, not owned or freed by the tree
    struct ll_tree_node *parent;
    struct ll_tree_node *first_child;
    struct ll_tree_node *next_sibling;
    struct ll_tree_node *prev_sibling;
};

struct ll_tree {
    struct ll_tree_node *root;
};

// Initialize an existing tree container (e.g., on the stack).
// Does not allocate any nodes.
void ll_tree_init(struct ll_tree *tree);

// Create a new tree node with the given kind and payload.
// Returns NULL on allocation failure.
struct ll_tree_node *ll_tree_node_create(int kind, void *payload);

// Free a single node (does NOT touch its children or siblings).
// Does not free the payload. Only the node struct itself.
void ll_tree_node_free(struct ll_tree_node *node);

// Iteratively free a subtree rooted at 'node' using an explicit stack.
// For each node, calls cleanup(kind, payload) if cleanup != NULL.
// Does not modify any tree->root; caller must detach the subtree
// from its parent before freeing if needed.
void ll_tree_free_subtree(struct ll_tree_node *node,
                          void (*cleanup)(int kind, void *payload));

// Set the root node of the tree.
// If root is NULL, the tree becomes empty.
// If root is non-NULL, its parent and sibling links are cleared.
void ll_tree_set_root(struct ll_tree *tree, struct ll_tree_node *root);

// True if the tree has no root.
int ll_tree_is_empty(const struct ll_tree *tree);

// Attach 'child' as the last child of 'parent'.
// Both pointers must be non-NULL.
// 'child' must be detached before calling (no parent and no siblings).
// The subtree under child->first_child is not modified.
void ll_tree_add_child(struct ll_tree_node *parent,
                       struct ll_tree_node *child);

// Attach 'child' as the first child of 'parent'.
// Both pointers must be non-NULL.
// 'child' must be detached before calling.
void ll_tree_add_child_front(struct ll_tree_node *parent,
                             struct ll_tree_node *child);

// Detach 'node' (and its entire subtree) from its parent and siblings.
// After this call, node has parent == NULL and no sibling links.
// Children under node->first_child remain attached to node.
void ll_tree_detach(struct ll_tree_node *node);

// True if node has no children.
int ll_tree_node_is_leaf(const struct ll_tree_node *node);

// True if node is the root of the given tree.
int ll_tree_node_is_root(const struct ll_tree *tree,
                         const struct ll_tree_node *node);

// Iterate over direct children of 'parent' from left to right (X axis).
void ll_tree_for_each_child(struct ll_tree_node *parent,
                            void (*fn)(struct ll_tree_node *));

// Apply fn() to 'node' and all its next_sibling chain (one "row").
void ll_tree_walk_siblings(struct ll_tree_node *node,
                           void (*fn)(struct ll_tree_node *node));

// Level-style traversal using an explicit stack.
void ll_tree_traverse_levels(struct ll_tree_node *root,
                             void (*fn)(struct ll_tree_node *node));
