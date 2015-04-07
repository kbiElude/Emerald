/**
 *
 * Emerald (kbi/elude @2012-2014)
 *
 */
#include "shared.h"
#include "system/system_bst.h"
#include "system/system_linear_alloc_pin.h"

/** Internal type definitions */
/** TODO */
typedef struct _system_bst_node _system_bst_node;

struct _system_bst_node
{
    system_bst_key    key;
    system_bst_value  value;

    _system_bst_node* left;
    _system_bst_node* right;
};

/** TODO */
typedef struct
{
    system_linear_alloc_pin      key_allocator;
    system_bst_value_equals_func key_equals_func;
    system_bst_value_lower_func  key_lower_func;
    size_t                       key_size;
    system_linear_alloc_pin      node_allocator;
    _system_bst_node             root;
    system_linear_alloc_pin      value_allocator;
    size_t                       value_size;
} _system_bst;

/** TODO */
PRIVATE void _init_system_bst_node(__in __notnull _system_bst_node* node_ptr,
                                   __in __notnull system_bst_key    key,
                                   __in __notnull system_bst_value  value)
{
    node_ptr->key   = key;
    node_ptr->value = value;
    node_ptr->left  = NULL;
    node_ptr->right = NULL;
}


/* Please see header for specification */
PUBLIC EMERALD_API system_bst system_bst_create(__in           size_t                       key_size,
                                                __in           size_t                       value_size,
                                                __in __notnull system_bst_value_lower_func  key_lower_func,
                                                __in __notnull system_bst_value_equals_func key_equals_func,
                                                __in __notnull system_bst_key               initial_key,
                                                __in __notnull system_bst_value             initial_value)
{
    _system_bst* new_instance = new (std::nothrow) _system_bst;

    ASSERT_ALWAYS_SYNC(new_instance != NULL, "Out of memory");
    if (new_instance != NULL)
    {
        new_instance->key_allocator   = system_linear_alloc_pin_create(key_size, 512, 1);
        new_instance->key_equals_func = key_equals_func;
        new_instance->key_lower_func  = key_lower_func;
        new_instance->key_size        = key_size;
        new_instance->node_allocator  = system_linear_alloc_pin_create(sizeof(_system_bst_node), 512, 1);
        new_instance->value_allocator = system_linear_alloc_pin_create(value_size, 512, 1);
        new_instance->value_size      = value_size;

        /* Create key & value copies */
        void* key_copy   = system_linear_alloc_pin_get_from_pool(new_instance->key_allocator);
        void* value_copy = system_linear_alloc_pin_get_from_pool(new_instance->value_allocator);

        memcpy(key_copy,   initial_key,   key_size);
        memcpy(value_copy, initial_value, value_size);

        _init_system_bst_node(&new_instance->root,
                              (system_bst_key) key_copy,
                              (system_bst_value) value_copy);
    }

    return (system_bst) new_instance;
}

/* Please see header for specification */
PUBLIC EMERALD_API bool system_bst_get(__in  __notnull   system_bst        bst,
                                       __in  __notnull   system_bst_key    key,
                                       __out __maybenull system_bst_value* result)
{
    _system_bst*      bst_ptr        = (_system_bst*) bst;
    _system_bst_node* current_node   = &bst_ptr->root;
    bool              logical_result = false;

    while (current_node != NULL)
    {
        if (bst_ptr->key_equals_func(bst_ptr->key_size,
                                     current_node->key,
                                     key) )
        {
            if (result != NULL)
            {
                memcpy(result,
                       current_node->value,
                       bst_ptr->value_size);
            }

            logical_result = true;
            break;
        }

        if (bst_ptr->key_lower_func(bst_ptr->key_size,
                                    current_node->key,
                                    key) )
        {
            if (current_node->left != NULL)
            {
                current_node = current_node->left;
            }
            else
            {
                break;
            }
        }
        else
        {
            if (current_node->right != NULL)
            {
                current_node = current_node->right;
            }
            else
            {
                break;
            }
        }
    }

    return logical_result;
}

/* Please see header for specification */
PUBLIC EMERALD_API void system_bst_insert(__in __notnull system_bst       bst,
                                          __in __notnull system_bst_key   key,
                                          __in __notnull system_bst_value value)
{
    _system_bst*      bst_ptr       = (_system_bst*) bst;
    _system_bst_node* current_node  = &bst_ptr->root;
    bool              is_lower_eq   = false;
    _system_bst_node* previous_node = current_node;
    
    while (current_node != NULL)
    {
        previous_node = current_node;

        if (bst_ptr->key_lower_func(bst_ptr->key_size,
                                    current_node->key,
                                    key) )
        {
            current_node = current_node->left;
            is_lower_eq  = true;
        }
        else
        {
            current_node = current_node->right;
            is_lower_eq  = false;
        }
    }

    /* Create key/value copies */
    system_bst_key   key_copy   = (system_bst_key)   system_linear_alloc_pin_get_from_pool(bst_ptr->key_allocator);
    system_bst_value value_copy = (system_bst_value) system_linear_alloc_pin_get_from_pool(bst_ptr->value_allocator);

    memcpy(key_copy,
           key,
           bst_ptr->key_size);
    memcpy(value_copy,
           value,
           bst_ptr->value_size);

    if (is_lower_eq)
    {
        previous_node->left = (_system_bst_node*) system_linear_alloc_pin_get_from_pool(bst_ptr->node_allocator);

        _init_system_bst_node(previous_node->left,
                              key_copy,
                              value_copy);
    }
    else
    {
        previous_node->right = (_system_bst_node*) system_linear_alloc_pin_get_from_pool(bst_ptr->node_allocator);

        _init_system_bst_node(previous_node->right,
                              key_copy,
                              value_copy);
    }
}

/* Please see header for specification */
PUBLIC EMERALD_API void system_bst_release(__in __notnull system_bst bst)
{
    _system_bst* bst_ptr = (_system_bst*) bst;

    if (bst_ptr->key_allocator != NULL)
    {
        system_linear_alloc_pin_release(bst_ptr->key_allocator);
    }

    if (bst_ptr->node_allocator != NULL)
    {
        system_linear_alloc_pin_release(bst_ptr->node_allocator);
    }

    if (bst_ptr->value_allocator != NULL)
    {
        system_linear_alloc_pin_release(bst_ptr->value_allocator);
    }

    delete bst_ptr;
}