/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#ifndef CURVE_TYPES_H
#define CURVE_TYPES_H

#include "system/system_types.h"


/* Opaque curve segment-specific data type definition */
DECLARE_HANDLE(curve_segment_data);

/* Opaque curve segment type definition */
DECLARE_HANDLE(curve_segment);

/* Opaque curve container type definition */
DECLARE_HANDLE(curve_container);

/* Id of a single curve segment */
typedef uint32_t curve_segment_id;

/* Id of a curve segment node */
typedef uint32_t curve_segment_node_id;

/* Allowed boundary behaviors for curves */
typedef enum
{

    CURVE_CONTAINER_BOUNDARY_BEHAVIOR_UNDEFINED,
    CURVE_CONTAINER_BOUNDARY_BEHAVIOR_RESET,
    CURVE_CONTAINER_BOUNDARY_BEHAVIOR_REPEAT,
    CURVE_CONTAINER_BOUNDARY_BEHAVIOR_CONSTANT,

} curve_container_envelope_boundary_behavior;

/* Curve container properties */
typedef enum
{
    CURVE_CONTAINER_PROPERTY_DATA_TYPE,                /* not settable, system_variant_type */
    CURVE_CONTAINER_PROPERTY_LENGTH,                   /* not settable, system_timeline_time */
    CURVE_CONTAINER_PROPERTY_N_SEGMENTS,               /* not settable, uint32_t */
    CURVE_CONTAINER_PROPERTY_NAME,                     /* not settable, system_hashed_ansi_string */
    CURVE_CONTAINER_PROPERTY_POST_BEHAVIOR,            /* settable,     curve_container_envelope_boundary_behavior */
    CURVE_CONTAINER_PROPERTY_PRE_BEHAVIOR,             /* settable,     curve_container_envelope_boundary_behavior */
    CURVE_CONTAINER_PROPERTY_PRE_POST_BEHAVIOR_STATUS, /* settable,     bool */
    CURVE_CONTAINER_PROPERTY_START_TIME,               /* not settable, system_timeline_time */
} curve_container_property;

/* Curve container segment properties */
typedef enum
{
    CURVE_CONTAINER_SEGMENT_PROPERTY_N_NODES,          /* not settable, uint32_t */
    CURVE_CONTAINER_SEGMENT_PROPERTY_START_TIME,       /* settable,     system_timeline_time */
    CURVE_CONTAINER_SEGMENT_PROPERTY_END_TIME,         /* settable,     system_timeline_time */
    CURVE_CONTAINER_SEGMENT_PROPERTY_TYPE,             /* not settable, curve_segment_type */
    CURVE_CONTAINER_SEGMENT_PROPERTY_THRESHOLD,        /* settable,     float */
} curve_container_segment_property;

/* Allowed curve segment types */
typedef enum
{
    CURVE_SEGMENT_STATIC,
    CURVE_SEGMENT_LERP,
    CURVE_SEGMENT_TCB,

    CURVE_SEGMENT_UNDEFINED
} curve_segment_type;

/* Allowed curve segment properties */
typedef enum
{
    CURVE_SEGMENT_PROPERTY_TCB_START_CONTINUITY,
    CURVE_SEGMENT_PROPERTY_TCB_START_TENSION,
    CURVE_SEGMENT_PROPERTY_TCB_START_BIAS,

    CURVE_SEGMENT_PROPERTY_TCB_END_CONTINUITY,
    CURVE_SEGMENT_PROPERTY_TCB_END_TENSION,
    CURVE_SEGMENT_PROPERTY_TCB_END_BIAS,

} curve_segment_property;

/* TODO */
typedef enum
{
    CURVE_SEGMENT_NODE_PROPERTY_CONTINUITY,
    CURVE_SEGMENT_NODE_PROPERTY_TENSION,
    CURVE_SEGMENT_NODE_PROPERTY_BIAS
} curve_segment_node_property;

/* Helper macros */
#define MIN(x,y) (x < y ? x : y)
#define MAX(x,y) (x > y ? x : y)

/* Internal: Function pointer type definitions to various segment-specific handlers */
typedef bool (*PFNCURVESEGMENTADDNODE)            (curve_segment_data, system_timeline_time, system_variant, curve_segment_node_id*);
typedef bool (*PFNCURVESEGMENTDELETENODE)         (curve_segment_data, curve_segment_node_id);
typedef bool (*PFNCURVESEGMENTDEINIT)             (curve_segment_data);
typedef bool (*PFNCURVESEGMENTGETAMOUNTOFNODES)   (curve_segment_data, uint32_t*);
typedef bool (*PFNCURVESEGMENTGETNODE)            (curve_segment_data, curve_segment_node_id, system_timeline_time*, system_variant);
typedef bool (*PFNCURVESEGMENTGETNODEIDFORNODEAT) (curve_segment_data, uint32_t, curve_segment_node_id*);
typedef bool (*PFNCURVESEGMENTGETNODEPROPERTY)    (curve_segment_data, curve_segment_node_id, curve_segment_node_property, system_variant);
typedef bool (*PFNCURVESEGMENTGETPROPERTY)        (curve_segment_data, curve_segment_property, system_variant);
typedef bool (*PFNCURVESEGMENTGETNODEBYINDEX)     (curve_segment_data, uint32_t, curve_segment_node_id*);
typedef bool (*PFNCURVESEGMENTGETNODEINORDER)     (curve_segment_data, uint32_t, curve_segment_node_id*);
typedef bool (*PFNCURVESEGMENTGETVALUE)           (curve_segment_data, system_timeline_time, system_variant, bool);
typedef bool (*PFNCURVESEGMENTMODIFYNODEPROPERTY) (curve_segment_data, curve_segment_node_id, curve_segment_node_property, system_variant);
typedef bool (*PFNCURVESEGMENTMODIFYNODETIME)     (curve_segment_data, curve_segment_node_id, system_timeline_time);
typedef bool (*PFNCURVESEGMENTMODIFYNODETIMEVALUE)(curve_segment_data, curve_segment_node_id, system_timeline_time, system_variant, bool);
typedef bool (*PFNCURVESEGMENTSETPROPERTY)        (curve_segment_data, curve_segment_property, system_variant);

/* Callback function pointer definition */
typedef void (*PFNCURVESEGMENTONCURVECHANGED)(void* user_arg);

#endif /* CURVE_TYPES_H */
