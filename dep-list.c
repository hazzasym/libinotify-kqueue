/*******************************************************************************
  Copyright (c) 2011-2014 Dmitry Matveev <me@dmitrymatveev.co.uk>

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*******************************************************************************/

#include <errno.h>   /* errno */
#include <stddef.h>  /* offsetof */
#include <stdlib.h>  /* calloc */
#include <stdio.h>   /* printf */
#include <dirent.h>  /* opendir, readdir, closedir */
#include <string.h>  /* strcmp */
#include <assert.h>

#include "compat.h"
#include "utils.h"
#include "dep-list.h"

/**
 * Print a list to stdout.
 *
 * @param[in] dl A pointer to a list.
 **/
void
dl_print (const dep_list *dl)
{
    dep_node *dn;

    SLIST_FOREACH (dn, &dl->head, next) {
        printf ("%lld:%s ", (long long int) dn->item->inode, dn->item->path);
    }
    printf ("\n");
}

/**
 * Create a new list.
 *
 * Create a new list and initialize its fields.
 *
 * @return A pointer to a new list or NULL in the case of error.
 **/
dep_list*
dl_create ()
{
    dep_list *dl = calloc (1, sizeof (dep_list));
    if (dl == NULL) {
        perror_msg ("Failed to allocate new dep-list");
        return NULL;
    }
    SLIST_INIT (&dl->head);
    return dl;
}

/**
 * Create a new list node.
 *
 * Create a new list node and initialize its fields.
 *
 * @param[in] di Parent directory of depedence list.
 * @return A pointer to a new list or NULL in the case of error.
 **/
dep_node*
dn_create (dep_item *di)
{
    dep_node *dn = calloc (1, sizeof (dep_node));
    if (dn == NULL) {
        perror_msg ("Failed to allocate new dep-list node");
        return NULL;
    }
    dn->item = di;
    return dn;
}

/**
 * Create a new list item.
 *
 * Create a new list item and initialize its fields.
 *
 * @param[in] path  A name of a file (the string is not copied!).
 * @param[in] inode A file's inode number.
 * @return A pointer to a new item or NULL in the case of error.
 **/
dep_item*
di_create (const char *path, ino_t inode)
{
    size_t pathlen = strlen (path) + 1;

    dep_item *di = calloc (1, offsetof (dep_item, path) + pathlen);
    if (di == NULL) {
        perror_msg ("Failed to create a new dep-list item");
        return NULL;
    }

    strlcpy (di->path, path, pathlen);
    di->inode = inode;
    return di;
}

/**
 * Insert new item into list.
 *
 * @param[in] dl A pointer to a list.
 * @param[in] di A pointer to a list item to be inserted.
 * @return A pointer to a new node or NULL in the case of error.
 **/
dep_node*
dl_insert (dep_list* dl, dep_item* di)
{
    dep_node *dn = dn_create (di);
    if (dn == NULL) {
        perror_msg ("Failed to create a new dep-list node");
        return NULL;
    }

    SLIST_INSERT_HEAD (&dl->head, dn, next);

    return dn;
}

/**
 * Remove item after specified from a list.
 *
 * @param[in] dl      A pointer to a list.
 * @param[in] prev_dn A pointer to a list node prepending removed one.
 *     Should be NULL to remove first node from a list.
 **/
void
dl_remove_after (dep_list* dl, dep_node* prev_dn)
{
    dep_node *dn;

    if (prev_dn) {
        dn = SLIST_NEXT (prev_dn, next);
        SLIST_REMOVE_AFTER (prev_dn, next);
    } else {
        dn = SLIST_FIRST (&dl->head);
        SLIST_REMOVE_HEAD (&dl->head, next);
    }

    free (dn);
}

/**
 * Create a shallow copy of a list.
 *
 * A shallow copy is a copy of a structure, but not the copy of the
 * contents. All data pointers (`item' in our case) of a list and its
 * shallow copy will point to the same memory.
 *
 * @param[in] dl A pointer to list to make a copy. May be NULL.
 * @return A shallow copy of the list.
 **/ 
dep_list*
dl_shallow_copy (const dep_list *dl)
{
    assert (dl != NULL);

    dep_list *head = dl_create ();
    if (head == NULL) {
        perror_msg ("Failed to allocate head during shallow copy");
        return NULL;
    }

    dep_node *cp = NULL;
    dep_node *iter;

    SLIST_FOREACH (iter, &dl->head, next) {
        dep_node *dn = dn_create (iter->item);
        if (dn == NULL) {
                perror_msg ("Failed to allocate a new element during shallow copy");
                dl_shallow_free (head);
                return NULL;
        }

        if (cp == NULL) {
            SLIST_INSERT_HEAD (&head->head, dn, next);
        } else {
            SLIST_INSERT_AFTER (cp, dn, next);
        }
        cp = dn;
    }

    return head;
}

/**
 * Free the memory allocated for shallow copy.
 *
 * This function will free the memory used by a list structure, but
 * the list data will remain in the heap.
 *
 * @param[in] dl A pointer to a list. May be NULL.
 **/
void
dl_shallow_free (dep_list *dl)
{
    assert (dl != NULL);

    dep_node *ptr, *tmp;

    SLIST_FOREACH_SAFE (ptr, &dl->head, next, tmp) {
        free (ptr);
    }

    free (dl);
}

/**
 * Free the memory allocated for a list item.
 *
 * This function will free the memory used by a list item.
 *
 * @param[in] dn A pointer to a list item. May be NULL.
 **/
void
di_free (dep_item *di)
{
    free (di);
}

/**
 * Free the memory allocated for a list.
 *
 * This function will free all the memory used by a list: both
 * list structure and the list data.
 *
 * @param[in] dl A pointer to a list.
 **/
void
dl_free (dep_list *dl)
{
    assert (dl != NULL);

    dep_node *iter, *tmp;

    SLIST_FOREACH_SAFE (iter, &dl->head, next, tmp) {
        di_free (iter->item);
        free (iter);
    }

    free (dl);
}

/**
 * Create a directory listing and return it as a list.
 *
 * @param[in] path A path to a directory.
 * @return A pointer to a list. May return NULL, check errno in this case.
 **/
dep_list*
dl_listing (const char *path)
{
    assert (path != NULL);

    dep_list *head = dl_create ();
    if (head == NULL) {
        perror_msg ("Failed to allocate list during directory listing");
        return NULL;
    }

    DIR *dir = opendir (path);
    if (dir == NULL && errno != ENOENT) {
        /* Why do I skip ENOENT? Because the directory could be deleted at this
         * point */
         perror_msg ("Failed to open directory %s during listing", path);
         goto error;
    }

    if (dir != NULL) {
        struct dirent *ent;
        dep_item *item;
        dep_node *node;

        while ((ent = readdir (dir)) != NULL) {
            if (!strcmp (ent->d_name, ".") || !strcmp (ent->d_name, "..")) {
                continue;
            }

            item = di_create (ent->d_name, ent->d_ino);
            if (item == NULL) {
                perror_msg ("Failed to allocate a new item during listing");
                goto error;
            }

            node = dl_insert (head, item);
            if (node == NULL) {
                free (item);
                perror_msg ("Failed to allocate a new node during listing");
                goto error;
            }
        }

        closedir (dir);
    }
    return head;

error:
    if (dir != NULL) {
        closedir (dir);
    }
    dl_free (head);
    return NULL;
}

/**
 * Perform a diff on lists.
 *
 * This function performs something like a set intersection. The same items
 * will be removed from the both lists. Items are comapred by a filename.
 * 
 * @param[in] before A pointer to a pointer to a list. Will contain items
 *     which were not found in the `after' list.
 * @param[in] after  A pointer to a pointer to a list. Will contain items
 *     which were not found in the `before' list.
 **/
void
dl_diff (dep_list *before, dep_list *after)
{
    assert (before != NULL);
    assert (after != NULL);

    dep_node *before_iter, *tmp;
    dep_node *before_prev = NULL;

    SLIST_FOREACH_SAFE (before_iter, &before->head, next, tmp) {
        dep_node *after_iter;
        dep_node *after_prev = NULL;

        int matched = 0;
        SLIST_FOREACH (after_iter, &after->head, next) {
            if (strcmp (before_iter->item->path, after_iter->item->path) == 0) {
                matched = 1;
                /* removing the entry from the both lists */
                dl_remove_after (before, before_prev);
                dl_remove_after (after, after_prev);
                break;
            }
            after_prev = after_iter;
        }

        if (matched == 0) {
            before_prev = before_iter;
        }
    }
}


/**
 * Traverses two lists. Compares items with a supplied expression
 * and performs the passed code on a match. Removes the matched entries
 * from the both lists.
 **/
#define EXCLUDE_SIMILAR(removed_list, added_list, match_expr, matched_code) \
    dep_node *removed_list##_iter, *tmp;                                \
    dep_node *removed_list##_prev = NULL;                               \
                                                                        \
    int productive = 0;                                                 \
                                                                        \
    SLIST_FOREACH_SAFE (removed_list##_iter, &removed_list->head, next, tmp) { \
        dep_node *added_list##_iter;                                    \
        dep_node *added_list##_prev = NULL;                             \
                                                                        \
        int matched = 0;                                                \
        SLIST_FOREACH (added_list##_iter, &added_list->head, next) {    \
            if (match_expr) {                                           \
                matched = 1;                                            \
                ++productive;                                           \
                matched_code;                                           \
                                                                        \
                dl_remove_after (removed_list, removed_list##_prev);    \
                dl_remove_after (added_list, added_list##_prev);        \
                break;                                                  \
            }                                                           \
            added_list##_prev = added_list##_iter;                      \
        }                                                               \
        if (matched == 0) {                                             \
            removed_list##_prev = removed_list##_iter;                  \
        }                                                               \
    }                                                                   \
    return (productive > 0);


#define cb_invoke(cbs, name, udata, ...) \
    do { \
        if (cbs->name) { \
            (cbs->name) (udata, ## __VA_ARGS__); \
        } \
    } while (0)

/**
 * Detect and notify about files remained unmoved between directory scans
 *
 * This function produces symmetric diffrence of two sets. The same items will
 * be removed from the both lists. Items are compared by name and inode number.
 * 
 * @param[in] before A pointer to a list. Will contain items
 *     which were not found in the `after' list.
 * @param[in] after  A pointer to a list. Will contain items
 *     which were not found in the `before' list.
 * @param[in] cbs    A pointer to #traverse_cbs, an user-defined set of
 *     traverse callbacks.
 * @param[in] udata  A pointer to the user-defined data.
 **/
static int
dl_detect_unchanged (dep_list            *before,
                     dep_list            *after,
                     const traverse_cbs  *cbs,
                     void                *udata)
{
    assert (cbs != NULL);

    EXCLUDE_SIMILAR
        (before, after,
         (before_iter->item->inode == after_iter->item->inode &&
          strcmp(before_iter->item->path, after_iter->item->path) == 0),
         {
             cb_invoke (cbs, unchanged, udata, before_iter->item, after_iter->item);
         });
}

/**
 * Detect and notify about moves in the watched directory.
 *
 * A move is what happens when you rename a file in a directory, and
 * a new name is unique, i.e. you didnt overwrite any existing files
 * with this one.
 *
 * @param[in] removed  A list of the removed files in the directory.
 * @param[in] added    A list of the added files of the directory.
 * @param[in] cbs      A pointer to #traverse_cbs, an user-defined set of
 *     traverse callbacks.
 * @param[in] udata    A pointer to the user-defined data.
 * @return 0 if no files were renamed, >0 otherwise.
**/
static int
dl_detect_moves (dep_list            *removed, 
                 dep_list            *added, 
                 const traverse_cbs  *cbs, 
                 void                *udata)
{
    assert (cbs != NULL);

     EXCLUDE_SIMILAR
        (removed, added,
         (removed_iter->item->inode == added_iter->item->inode),
         {
             cb_invoke (cbs, moved, udata, removed_iter->item, added_iter->item);
         });
}

/**
 * Detect and notify about replacements in the watched directory.
 *
 * Consider you are watching a directory foo with the folloing files
 * insinde:
 *
 *    foo/bar
 *    foo/baz
 *
 * A replacement in a watched directory is what happens when you invoke
 *
 *    mv /foo/bar /foo/bar
 *
 * i.e. when you replace a file in a watched directory with another file
 * from the same directory.
 *
 * @param[in] removed  A list of the removed files in the directory.
 * @param[in] current  A list with the current contents of the directory.
 * @param[in] cbs      A pointer to #traverse_cbs, an user-defined set of
 *     traverse callbacks.
 * @param[in] udata    A pointer to the user-defined data.
 * @return 0 if no files were renamed, >0 otherwise.
 **/
static int
dl_detect_replacements (dep_list            *removed,
                        dep_list            *current,
                        const traverse_cbs  *cbs,
                        void                *udata)
{
    assert (cbs != NULL);

    EXCLUDE_SIMILAR
        (removed, current,
         (removed_iter->item->inode == current_iter->item->inode),
         {
            cb_invoke (cbs, replaced, udata, removed_iter->item, current_iter->item);
         });
}

/**
 * Detect and notify about overwrites in the watched directory.
 *
 * Consider you are watching a directory foo with a file inside:
 *
 *    foo/bar
 *
 * And you also have a directory tmp with a file 1:
 * 
 *    tmp/1
 *
 * You do not watching directory tmp.
 *
 * An overwrite in a watched directory is what happens when you invoke
 *
 *    mv /tmp/1 /foo/bar
 *
 * i.e. when you overwrite a file in a watched directory with another file
 * from the another directory.
 *
 * @param[in] previous A list with the previous contents of the directory.
 * @param[in] current  A list with the current contents of the directory.
 * @param[in] cbs      A pointer to #traverse_cbs, an user-defined set of
 *     traverse callbacks.
 * @param[in] udata    A pointer to the user-defined data.
 * @return 0 if no files were renamed, >0 otherwise.
 **/
static int
dl_detect_overwrites (dep_list            *previous,
                      dep_list            *current,
                      const traverse_cbs  *cbs,
                      void                *udata)
{
    assert (cbs != NULL);

    EXCLUDE_SIMILAR
        (previous, current,
         (strcmp (previous_iter->item->path, current_iter->item->path) == 0
          && previous_iter->item->inode != current_iter->item->inode),
         {
             cb_invoke (cbs, overwritten, udata, current_iter->item);
         });
}


/**
 * Traverse a list and invoke a callback for each item.
 * 
 * @param[in] list  A #dep_list.
 * @param[in] cb    A #single_entry_cb callback function.
 * @param[in] udata A pointer to the user-defined data.
 **/
static void 
dl_emit_single_cb_on (dep_list        *list,
                      single_entry_cb  cb,
                      void            *udata)
{
    dep_node *iter;

    if (cb == NULL)
        return;

    SLIST_FOREACH (iter, &list->head, next) {
        (cb) (udata, iter->item);
    }
}


/**
 * Recognize all the changes in the directory, invoke the appropriate callbacks.
 *
 * This is the core function of directory diffing submodule.
 *
 * @param[in] before The previous contents of the directory.
 * @param[in] after  The current contents of the directory.
 * @param[in] cbs    A pointer to user callbacks (#traverse_callbacks).
 * @param[in] udata  A pointer to user data.
 **/
void
dl_calculate (dep_list           *before,
              dep_list           *after,
              const traverse_cbs *cbs,
              void               *udata)
{
    assert (cbs != NULL);

    int need_update = 0;

    dep_list *was = dl_shallow_copy (before);
    dep_list *now = dl_shallow_copy (after);

    dl_detect_unchanged (was, now, cbs, udata);

    dep_list *pre = dl_shallow_copy (was);
    dep_list *lst = dl_shallow_copy (now);

    dl_diff (was, now);

    need_update += dl_detect_moves (was, now, cbs, udata);
    need_update += dl_detect_replacements (was, lst, cbs, udata);
    dl_detect_overwrites (pre, lst, cbs, udata);
 
    if (need_update) {
        cb_invoke (cbs, names_updated, udata);
    }

    dl_emit_single_cb_on (was, cbs->removed, udata);
    dl_emit_single_cb_on (now, cbs->added, udata);

    cb_invoke (cbs, many_added, udata, now);
    cb_invoke (cbs, many_removed, udata, was);
    
    dl_shallow_free (lst);
    dl_shallow_free (now);
    dl_shallow_free (pre);
    dl_shallow_free (was);
}

