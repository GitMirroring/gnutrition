#ifndef I18N_H
#define I18N_H

#include <libintl.h>
#include <locale.h>

#define _(String) gettext (String)
#define N_(String) String

#endif /* I18N_H */
