// SPDX-License-Identifier: GPL-2.0-or-later
// libbigwm-malloc-once.so: carregada por LD_PRELOAD junto com um alocador,
// tira do ambiente as variáveis listadas em BIG_WM_MALLOC_UNSET (e ela
// mesma) logo ao carregar. Assim o alocador vale só para este processo: os
// programas que ele inicia não herdam o LD_PRELOAD nem o GLIBC_TUNABLES.
// Com BIG_WM_MALLOC_UNSET_IF_ARG, só no processo com esse argumento (o
// wmaker é um lançador que inicia o "wmaker --for-real": o ambiente passa
// para ele, que o tira para os seus filhos).
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/** @return Whether an argument of this process is `arg`. */
static bool has_arg(const char *arg)
{
    char buf[4096];
    int fd = open("/proc/self/cmdline", O_RDONLY | O_CLOEXEC);
    if (0 > fd) return false;
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (0 >= n) return false;
    buf[n] = '\0';
    for (ssize_t i = 0; i < n; i += strlen(&buf[i]) + 1) {
        if (0 == strcmp(&buf[i], arg)) return true;
    }
    return false;
}

__attribute__((constructor))
static void bigwm_malloc_once(void)
{
    const char *list = getenv("BIG_WM_MALLOC_UNSET");
    if (NULL == list) return;
    const char *only = getenv("BIG_WM_MALLOC_UNSET_IF_ARG");
    if (NULL != only && !has_arg(only)) return;
    char buf[256];
    size_t n = strlen(list);
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, list, n);
    buf[n] = '\0';
    char *save = NULL;
    for (char *v = strtok_r(buf, " ", &save); NULL != v;
         v = strtok_r(NULL, " ", &save)) {
        unsetenv(v);
    }
    unsetenv("BIG_WM_MALLOC_UNSET");
    unsetenv("BIG_WM_MALLOC_UNSET_IF_ARG");
}
