# LavaLite C Coding Style Guide

This document defines the **official C coding style** for the LavaLite project.

The goals are:

* consistent formatting
* modern readable C
* no legacy OpenLava/LSF quirks
* fully enforceable via `.clang-format`

All new code must follow this style.

---

## 1. Using `.clang-format` (Mandatory)

The repository contains a `.clang-format` file that defines the exact formatting rules.

Before committing, format all modified files using

```
clang-format
```

Format a single file:

```
clang-format -i src/file.c
```

Format the entire repository:

```
find . -name '*.[ch]' -exec clang-format -i {} +
```

**Do not manually adjust formatting.**
The `.clang-format` file is the **single source of truth**.

Pull requests not following it may be rejected.

---

## 2. Indentation & Whitespace

* Indent with **4 spaces** (never tabs)
* No trailing whitespace
* Soft column limit: **80 characters**

Example:

```
if (cond) {
    do_something();
}
```

---

## 3. Braces & Layout

We use a **Linux/K&R–style brace layout**.

### Functions

```
int ll_host_is_ok(struct ll_host *h)
{
    if (!h) {
        return 0;
    }

    return h->state == HOST_OK;
}
```

### Control Structures

Always use braces, even for single-line bodies:

```
if (x) {
    return 1;
}
```

Incorrect:

```
if (x) return 1;
if (x)
    return 1;
```

---

## 4. Comments

* Use `//` for standard comments.
* Use `/* ... */` only for:

  * license headers
  * large documentation blocks

Example:

```c
// Fast path: host already checked
```

---

## 5. Function Definitions & Prototypes

### In `.c` files:

```
int ll_queue_init(struct ll_queue *q)
{
    ...
}
```

### In header files:

Omit parameter names:

```
int ll_queue_init(struct ll_queue *);
int ll_host_lookup(struct ll_host *, const char *);
```

This avoids header churn.

---

## 6. Conditionals & Loops

* Braces are mandatory.
* Conditions should be readable.

```
while (node != NULL) {
    process_node(node);
    node = node->next;
}
```

---

## 7. Variable Declarations

Declare **one variable per line**.

Correct:

```
int argc;
int index;

char *path;
char *host;
```

Incorrect:

```
int argc, index;
char *path, *host;
```

This improves clarity and simplifies diffs.

---

## 8. Boolean Type

Use **`bool_t`** everywhere in the codebase.

Do **not** use C99 `bool` or C++ `bool`.
Do **not** include `<stdbool.h>`.

`bool_t` comes from XDR/TIRPC and is defined in:

```
#include <rpc/types.h>
```

which provides:

```
typedef int bool_t;
#define TRUE 1
#define FALSE 0
```

### Lowercase Boolean Literals

LavaLite uses lowercase literals for stylistic consistency:

```
true
false
```

These are defined internally as:

```
#define true  TRUE
#define false FALSE
```

This keeps the entire codebase uniform while avoiding C99 `_Bool`.

### Correct Usage

```
bool_t ok = true;
if (ok) {
    return false;
}
```

### Incorrect Usage

```
bool ok = true;       // wrong type
bool_t ok = TRUE;     // avoid uppercase literals
false == FALSE;       // mixing styles
```

---

## 10. Pointer Style

Pointer asterisk binds to the variable name:

```
char *name;
struct ll_host *host;
```

---

## 9. Include Ordering

1. System headers (`<...>`)
2. The file’s own header (if any)
3. Other project headers


Example:

```
#include <stdlib.h>
#include <string.h>

#include "ll_host.h"
#include "ll_queue.h"
```
**Note**:

Include ordering is intentionally not modified by clang-format (SortIncludes: false). The order is significant for LavaLite due to legacy dependencies, so it must remain unchanged for now.

---


## 10. Error Handling

* Continue using **thread-local `lserrno`**
* Set it before returning on errors

```
if (!host) {
    lserrno = LLE_NO_HOST;
    return -1;
}
```

Use fixed-size integer types (`uint32_t`, `int64_t`, etc.) for protocol fields.

---

## 11. Miscellaneous Rules

* Prefer `static` for internal functions
* Avoid complex macros
* Prefer `enum` or `static const` over `#define` when appropriate
* Keep functions reasonably short
* Avoid unnecessary cleverness

---

## 12. Philosophy

* **Consistency over personal style**
* **Clarity over cleverness**
* Small, readable diffs
* Explicit is better than implicit

When in doubt knock em out, run:

```
clang-format
```

And trust the tool.

Welcome to the LavaLite coding style.
