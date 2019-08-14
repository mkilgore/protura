/*
 * Copyright (C) 2013 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/mm/kmalloc.h>
#include <protura/rbtree.h>

static inline void rb_change_child(struct rbtree *tree, struct rbnode *old, struct rbnode *new, struct rbnode *parent)
{
    if (parent) {
        if (parent->left == old)
            parent->left = new;
        else
            parent->right = new;
    } else {
        tree->root = new;
    }
}

static inline void rb_rotate_set_parents(struct rbtree *tree, struct rbnode *old, struct rbnode *new, int color)
{
    struct rbnode *parent = rb_parent(old);
    new->_parent_color = old->_parent_color;
    rb_set_parent_color(old, new, color);
    rb_change_child(tree, old, new, parent);
}

static void rb_insert_rebalance(struct rbtree *tree, struct rbnode *node)
{
    struct rbnode *parent = rb_parent(node), *gparent, *tmp;

    rb_set_color(node, RB_RED);
    while (1) {

        if (!parent) {
            rb_set_color(node, RB_BLACK);
            break;
        } else if (rb_color(parent) == RB_BLACK) {
            break;
        }

        gparent = rb_parent(parent);

        tmp = gparent->right;
        if (parent != tmp) {
            if (tmp && rb_color(tmp) == RB_RED) {
                rb_set_parent_color(tmp, gparent, RB_BLACK);
                rb_set_parent_color(parent, gparent, RB_BLACK);
                node = gparent;
                parent = rb_parent(node);
                rb_set_parent_color(node, parent, RB_RED);
                continue;
            }

            tmp = parent->right;
            if (node == tmp) {
                tmp = node->left;
                parent->right = tmp;
                node->left = parent;
                if (tmp)
                    rb_set_parent_color(tmp, parent, RB_BLACK);
                rb_set_parent_color(parent, node, RB_RED);
                parent = node;
                tmp = node->right;
            }

            gparent->left = tmp;
            parent->right = gparent;
            if (tmp)
                rb_set_parent_color(tmp, gparent, RB_BLACK);
            rb_rotate_set_parents(tree, gparent, parent, RB_RED);
            break;
        } else {
            tmp = gparent->left;
            if (tmp && rb_color(tmp) == RB_RED) {
                rb_set_parent_color(tmp, gparent, RB_BLACK);
                rb_set_parent_color(parent, gparent, RB_BLACK);
                node = gparent;
                parent = rb_parent(node);
                rb_set_parent_color(node, parent, RB_RED);
                continue;
            }

            tmp = parent->left;
            if (node == tmp) {
                tmp = node->right;
                parent->left = tmp;
                node->right = parent;
                if (tmp)
                    rb_set_parent_color(tmp, parent, RB_BLACK);
                rb_set_parent_color(parent, node, RB_RED);
                parent = node;
                tmp = node->left;
            }

            gparent->right = tmp;
            parent->left = gparent;
            if (tmp)
                rb_set_parent_color(tmp, gparent, RB_BLACK);
            rb_rotate_set_parents(tree, gparent, parent, RB_RED);
            break;
        }
    }
}

int rb_insert(struct rbtree *tree, struct rbnode *newnode)
{
    struct rbnode *parent= NULL, **current_dbl = &tree->root;
    int comp = -1;

    while (*current_dbl) {
        comp = (tree->compare) (*current_dbl, newnode);
        parent = *current_dbl;
        if (comp == RB_GT)
            current_dbl = &((*current_dbl)->left);
        else if (comp == RB_LT)
            current_dbl = &((*current_dbl)->right);
        else
            return 0;
    }

    rb_set_parent_color(newnode, parent, RB_RED);
    newnode->left = NULL;
    newnode->right = NULL;

    *current_dbl = newnode;

    rb_insert_rebalance(tree, newnode);
    return 1;
}

static void remove_rebalance(struct rbtree *tree, struct rbnode *parent)
{
    struct rbnode *node = NULL, *sibling, *tmp1, *tmp2;

    while (1) {
        sibling = parent->right;
        if (node != sibling) {
            if (rb_color(sibling) == RB_RED) {
                tmp1 = sibling->left;
                parent->right = tmp1;
                sibling->left = parent;
                rb_set_parent_color(tmp1, parent, RB_BLACK);
                rb_rotate_set_parents(tree, parent, sibling, RB_RED);
                sibling = tmp1;
            }
            tmp1 = sibling->right;
            if (!tmp1 || rb_color(tmp1) == RB_BLACK) {
                tmp2 = sibling->left;
                if (!tmp2 || rb_color(tmp2) == RB_BLACK) {
                    rb_set_parent_color(sibling, parent, RB_RED);
                    if (rb_color(parent) == RB_RED) {
                        rb_set_color(parent, RB_BLACK);
                    } else {
                        node = parent;
                        parent = rb_parent(node);
                        if (parent)
                            continue;
                    }
                    break;
                }
                tmp1 = tmp2->right;
                sibling->left = tmp1;
                tmp2->right = sibling;
                parent->right = tmp2;
                if (tmp1)
                    rb_set_parent_color(tmp1, sibling, RB_BLACK);
                tmp1 = sibling;
                sibling = tmp2;
            }
            tmp2 = sibling->left;
            parent->right = tmp2;
            sibling->left = parent;
            rb_set_parent_color(tmp1, sibling, RB_BLACK);
            if (tmp2)
                rb_set_parent(tmp2, parent);
            rb_rotate_set_parents(tree, parent, sibling, RB_BLACK);
            break;
        } else {
            sibling = parent->left;
            if (rb_color(sibling) == RB_RED) {
                tmp1 = sibling->right;
                parent->left = tmp1;
                sibling->right = parent;
                rb_set_parent_color(tmp1, parent, RB_BLACK);
                rb_rotate_set_parents(tree, parent, sibling, RB_RED);
                sibling = tmp1;
            }
            tmp1 = sibling->left;
            if (!tmp1 || rb_color(tmp1) == RB_BLACK) {
                tmp2 = sibling->right;
                if (!tmp2 || rb_color(tmp2) == RB_BLACK) {
                    rb_set_parent_color(sibling, parent, RB_RED);
                    if (rb_color(parent) == RB_RED) {
                        rb_set_color(parent, RB_BLACK);
                    } else {
                        node = parent;
                        parent = rb_parent(node);
                        if (parent)
                            continue;
                    }
                    break;
                }
                tmp1 = tmp2->left;
                sibling->right = tmp1;
                tmp2->left = sibling;
                parent->left = tmp2;
                if (tmp1)
                    rb_set_parent_color(tmp1, sibling, RB_BLACK);
                tmp1 = sibling;
                sibling = tmp2;
            }
            tmp2 = sibling->right;
            parent->left = tmp2;
            sibling->right = parent;
            rb_set_parent_color(tmp1, sibling, RB_BLACK);
            if (tmp2)
                rb_set_parent(tmp2, parent);
            rb_rotate_set_parents(tree, parent, sibling, RB_BLACK);
            break;
        }
    }
}

void rb_remove(struct rbtree *tree, struct rbnode *node)
{
    struct rbnode *child = node->right, *tmp = node->left;
    struct rbnode *parent, *rebalance;
    uintptr_t parent_color;

    if (!tmp) {
        parent_color = node->_parent_color;
        parent = rb_get_parent(parent_color);
        rb_change_child(tree, node, child, parent);
        if (child) {
            child->_parent_color = parent_color;
            rebalance = NULL;
        } else {
            rebalance = (__rb_is_black(parent_color))? parent: NULL;
        }

    } else if (!child) {
        parent_color = node->_parent_color;
        tmp->_parent_color = parent_color;
        parent = rb_get_parent(parent_color);
        rb_change_child(tree, node, tmp, parent);
        rebalance = NULL;
    } else {
        struct rbnode *successor = child, *child2;
        tmp = child->left;

        if (!tmp) {
            parent = successor;
            child2 = successor->right;
        } else {
            do {
                parent = successor;
                successor = tmp;
                tmp = tmp->left;
            } while (tmp);
            child2 = successor->right;
            parent->left = child2;
            successor->right = child;
            rb_set_parent(child, successor);
        }

        tmp = node->left;
        successor->left = tmp;
        rb_set_parent(tmp, successor);

        parent_color = node->_parent_color;
        tmp = rb_parent(node);
        rb_change_child(tree, node, successor, tmp);
        if (child2) {
            successor->_parent_color = parent_color;
            rb_set_parent_color(child2, parent, RB_BLACK);
            rebalance = NULL;
        } else {
            uintptr_t pc2 = successor->_parent_color;
            successor->_parent_color = parent_color;
            rebalance = (__rb_is_black(pc2))? parent: NULL;
        }
    }

    if (rebalance)
        remove_rebalance(tree, rebalance);
}

struct rbnode *rb_search(struct rbtree *tree, const struct rbnode *cmpnode)
{
    struct rbnode *current, *next;
    enum rbcomp comp = -1;

    for (current = tree->root; current != NULL && comp != RB_EQ; current = next) {
        comp = (tree->compare) (current, cmpnode);
        if (comp == RB_GT)
            next = current->left;
        else if (comp == RB_LT)
            next = current->right;
        else
            break;
    }

    if (comp == RB_EQ)
        return current;
    else
        return NULL;
}

struct rbnode *rb_trav_first_preorder(struct rbtree *tree, struct rb_trav_state *state)
{
    memset(state, 0, sizeof(*state));
    state->current = tree->root;

    return state->current;
}

struct rbnode *rb_trav_next_preorder(struct rb_trav_state *state)
{
    while (state->current != NULL) {
        if (state->from == RB_FROM_NONE && state->current->left) {
            state->current = state->current->left;
            state->from = RB_FROM_NONE;
        } else if (state->from <= RB_FROM_LEFT && state->current->right) {
            state->current = state->current->right;
            state->from = RB_FROM_NONE;
        } else {
            state->parent = rb_parent(state->current);
            if (state->parent == NULL)
                return NULL;

            if (state->current == state->parent->left)
                state->from = RB_FROM_LEFT;
            else
                state->from = RB_FROM_RIGHT;
            state->current = state->parent;
        }

        if (state->from == RB_FROM_NONE)
            return state->current;
    }

    return NULL;
}

struct rbnode *rb_trav_first_inorder(struct rbtree *tree, struct rb_trav_state *state)
{
    memset(state, 0, sizeof(*state));
    state->current = tree->root;

    return rb_trav_next_inorder(state);
}

struct rbnode *rb_trav_next_inorder(struct rb_trav_state *state)
{
    while (state->current != NULL) {
        if (state->from == RB_FROM_NONE && state->current->left) {
            state->current = state->current->left;
        } else if (state->from == RB_FROM_NONE) {
            state->from = RB_FROM_LEFT;
        } else if ((state->from == RB_FROM_NONE || state->from == RB_FROM_LEFT) && state->current->right) {
            state->from = RB_FROM_NONE;
            state->current = state->current->right;
        } else {
            state->parent = rb_parent(state->current);
            if (state->parent == NULL)
                return NULL;

            if (state->current == state->parent->left)
                state->from = RB_FROM_LEFT;
            else
                state->from = RB_FROM_RIGHT;
            state->current = state->parent;
        }

        if (state->from == RB_FROM_LEFT)
            return state->current;
    }

    return NULL;
}

struct rbnode *rb_trav_first_postorder(struct rbtree *tree, struct rb_trav_state *state)
{
    memset(state, 0, sizeof(*state));
    state->current = tree->root;

    return rb_trav_next_postorder(state);
}

struct rbnode *rb_trav_next_postorder(struct rb_trav_state *state)
{
    if (state->parent != NULL)
        state->current = state->parent;
    else if (state->from == RB_FROM_RIGHT)
        return NULL;

    while (state->current != NULL) {

        if (state->from == RB_FROM_NONE && state->current->left) {
            state->current = state->current->left;
        } else if (state->from <= RB_FROM_LEFT && state->current->right) {
            state->from = RB_FROM_NONE;
            state->current = state->current->right;
        } else {
            state->parent = rb_parent(state->current);
            if (state->parent != NULL) {
                if (state->parent->left == state->current)
                    state->from = RB_FROM_LEFT;
                else
                    state->from = RB_FROM_RIGHT;
            } else {
                state->from = RB_FROM_RIGHT;
            }

            return state->current;
        }
    }

    return NULL;
}
