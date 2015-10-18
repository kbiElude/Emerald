#ifndef STAGE_INTRO_H
#define STAGE_INTRO_H

#include "shared.h"
#include "demo/demo_types.h"
#include "spinner.h"

/** TODO */
PUBLIC void stage_intro_enqueue(demo_loader   loader,
                                spinner_stage stage);

/** TODO */
PUBLIC void stage_intro_init(demo_app    app,
                             demo_loader loader);


#endif /* STAGE_INTRO_H */