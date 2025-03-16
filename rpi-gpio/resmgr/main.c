/*
 * $QNXLicenseC:
 * Copyright 2021, QNX Software Systems. All Rights Reserved.
 *
 * You must obtain a written license from and pay applicable license fees to QNX
 * Software Systems before you may reproduce, modify or distribute this software,
 * or any work that includes all or part of this software.   Free development
 * licenses are available for evaluation and non-commercial purposes.  For more
 * information visit http://licensing.qnx.com or email licensing@qnx.com.
 *
 * This file may contain contributions from others.  Please review this entire
 * file for other proprietary rights or license notices, as well as the QNX
 * Development Suite License Guide at http://licensing.qnx.com/license-guide/
 * for other information.
 * $
 */

/**
 * @file    main.c
 * @brief   GPIO resource manager for Raspberry Pi 3
 *
 * The resource manager publishes a directory with a per-GPIO-pin node, and one
 * extra node called 'msg'. The numeric nodes can be used to configure, read and
 * write pins using textual commands, making them handy to work with from the
 * shell. The 'msg' node controls all pins, and uses structured _IO_MSG messages.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <time.h>
#include <login.h>
#include <sys/neutrino.h>
#include <sys/iofunc.h>
#include <sys/dispatch.h>
#include <sys/procmgr.h>
#include <sys/mman.h>
#include <sys/rpi_gpio.h>
#include <aarch64/mmu.h>
#include <aarch64/rpi_gpio.h>
#include <secpol/secpol.h>
#include <secpol/ids.h>
#include "rpi_gpio_priv.h"

typedef struct gpio_entry   gpio_entry_t;

/**
 * A node under the mount point representing a single GPIO pin.
 */
struct gpio_entry
{
    iofunc_attr_t       attr;
    char                name[8];
    unsigned            gpio;
};

uint64_t                        base_paddr = 0xfe000000;
uint32_t volatile              *rpi_gpio_regs;
int                             verbose;
static char const              *shm_path = SHM_ANON;
static resmgr_connect_funcs_t   connect_funcs;
static resmgr_io_funcs_t        io_funcs;
static iofunc_attr_t            io_attr;

static gpio_entry_t             gpio_entries[RPI_GPIO_NUM + 1];

/**
 * Handles an _IO_CONNECT message.
 * This message can be sent for the directory, as a result of an opendir() call,
 * or to a particular node, from an open(), stat() or unlink() calls.
 * The function associates an open control block (OCB) object with the attribute
 * structure for the matching node. If no node exists and O_CREAT is specified,
 * a new one is created and added to the linked list of nodes.
 * @param   ctp     Message context
 * @param   msg     Connect message
 * @param   dirattr Pointer to the attribute structure for the directory
 * @param   extra   Opaque pointer from the resource manager library
 * @return  EOK if successful, error code otherwise
 */
static int
open_gpio(resmgr_context_t *ctp, io_open_t *msg, iofunc_attr_t *dirattr,
          void *extra)
{
    // An empty path means that the directory was opened (for a readdir() call).
    if (msg->connect.path[0] == '\0') {
        return iofunc_open_default(ctp, msg, dirattr, extra);
    }

    // Find an entry matching the requested one.
    gpio_entry_t    *entry;
    for (unsigned i = 0;
         i < (sizeof(gpio_entries) / sizeof(gpio_entries[0]));
         i++) {
        entry = &gpio_entries[i];
        if (strncmp(msg->connect.path, entry->name, msg->connect.path_len)
            == 0) {
            // Found.
            return iofunc_open_default(ctp, msg, &entry->attr, extra);
        }
    }

    return ENOENT;
}

// The size of the data in a dirent_extra_stat structure. Used to populate the
// d_datalen field in the dirent_extra structure.
#define EXTRA_STAT_LEN                                          \
    (sizeof(struct dirent_extra_stat) - sizeof(struct dirent_extra))

/**
 * Handles an _IO_READ message on the directory.
 * The reply is an array of dirent structures, one for each node under the
 * directory, beginning with the last offset read (typically 0).
 * @param   ctp     Message context
 * @param   msg     Read message
 * @param   ocb     Control block for the open file
 * @return  Value encoding the reply IOV
 */
static int
read_directory(resmgr_context_t *ctp, io_read_t *msg, iofunc_ocb_t *ocb)
{
    size_t              offset = 0;
    gpio_entry_t        *entry;
    struct dirent       *dir = (struct dirent *)msg;
    size_t              reply_len = 0;
    size_t              max_reply_len = msg->i.nbytes;
    int                 need_extra = 0;

    // Never go beyond the end of the resource manager buffer.
    if (max_reply_len > (ctp->msg_max_size - ctp->offset)) {
        max_reply_len = ctp->msg_max_size - ctp->offset;
    }

    if (msg->i.xtype & _IO_XFLAG_DIR_EXTRA_HINT) {
        need_extra = 1;
    }

    // Generate a reply populated with dirent records.
    for (unsigned i = 0;
         i < (sizeof(gpio_entries) / sizeof(gpio_entries[0]));
         i++) {
        entry = &gpio_entries[i];

        // Skip entries below the current OCB offset.
        if (offset < ocb->offset) {
            offset++;
            continue;
        }

        // Make sure we have enough room for a dirent structure header before we
        // start populating it.
        if ((reply_len + sizeof(struct dirent)) > max_reply_len) {
            break;
        }

        // Initialize the record.
        dir->d_offset = offset;
        dir->d_namelen = strlen(entry->name);

        // Determine the total size of the record.
        struct dirent_extra *extra = _DEXTRA_FIRST(dir);
        uintptr_t           rec_len = (uintptr_t)extra - (uintptr_t)dir;
        if (need_extra) {
            rec_len += sizeof(struct dirent_extra_stat);
        }

        // Now that we have calculated the total size of the structure, we can
        // determine if the reply buffer is big enough for it.
        if ((reply_len + rec_len) > max_reply_len) {
            break;
        }

        // Finish populating the record.
        strcpy(dir->d_name, entry->name);
        dir->d_reclen = rec_len;
        if (need_extra) {
            unsigned    format = _STAT_FORM_PREFERRED;
            unsigned    len;
            struct stat *stp = (struct stat *)(extra + 1);
            iofunc_stat_format(ctp, &entry->attr, &format, stp, &len);
            extra->d_datalen = EXTRA_STAT_LEN;
            extra->d_type = _DTYPE_LSTAT;
        }

        // Advance to the next dirent structure.
        dir = (struct dirent *)((char *)dir + rec_len);
        reply_len += rec_len;
        offset++;
    }

    // Reply to the caller.
    _IO_SET_READ_NBYTES(ctp, reply_len);
    ocb->offset = offset;
    return reply_len == 0 ? EOK : _RESMGR_PTR(ctp, msg, reply_len);
}

/**
 * Handles an _IO_READ message.
 * If the OCB refers to the directory, the function calls read_directory().
 * Otherwise, the data from the referenced node is copied into the reply buffer,
 * starting from the last offset (typically 0).
 * @param   ctp     Message context
 * @param   msg     Read message
 * @param   ocb     Control block for the open file
 * @return  Value encoding the reply IOV
 */
static int
read_gpio(resmgr_context_t *ctp, io_read_t *msg, iofunc_ocb_t *ocb)
{
    if (ocb->attr->mode & S_IFDIR) {
        return read_directory(ctp, msg, ocb);
    }

    gpio_entry_t   *entry = (gpio_entry_t *)ocb->attr;
    if (entry->gpio == -1) {
        // Can't read the message node.
        return ENXIO;
    }

    if (rpi_gpio_get_select(entry->gpio) & 1) {
        return ENXIO;
    }

    if (msg->i.nbytes == 0) {
        return EOK;
    }

    if (ocb->offset != 0) {
        return EOK;
    }

    unsigned const  reg = 13 + (entry->gpio / 32);
    unsigned const  off = entry->gpio % 32;
    if (rpi_gpio_regs[reg] & (1 << off)) {
        *(char *)msg = '1';
    } else {
        *(char *)msg = '0';
    }

    // Reply to the caller.
    ocb->offset = 1;
    _IO_SET_READ_NBYTES(ctp, 1);
    return _RESMGR_PTR(ctp, msg, 1);
}

/**
 * Handles an _IO_WRITE message.
 * Overwrites the data associated with the node referred to by the OCB.
 * @param   ctp     Message context
 * @param   msg     Write message
 * @param   ocb     Control block for the open file
 * @return  EOK if successful, error code otherwise
 */
static int
write_gpio(resmgr_context_t *ctp, io_write_t *msg, iofunc_ocb_t *ocb)
{
    // Can't write a directory.
    if (ocb->attr->mode & S_IFDIR) {
        return ENOTSUP;
    }

    gpio_entry_t    *entry = (gpio_entry_t *)ocb->attr;
    if (entry->gpio == -1) {
        // Can't write the message node.
        return ENXIO;
    }

    // Parse command.
    size_t const        ncmdbytes = ctp->size - sizeof(msg->i);
    char                cmd[16];
    memcpy(cmd, &msg->i + 1, ncmdbytes);
    if (ncmdbytes == 0) {
        _IO_SET_WRITE_NBYTES(ctp, ncmdbytes);
        return EOK;
    }

    // Remove a trailing newline.
    if (cmd[ncmdbytes - 1] == '\n') {
        cmd[ncmdbytes - 1] = '\0';
    } else {
        cmd[ncmdbytes] = '\0';
    }

    if (verbose) {
        printf("Command=%s\n", cmd);
    }

    if (strcmp(cmd, "out") == 0) {
        // Set the GPIO as output.
        rpi_gpio_set_select(entry->gpio, 1);
    } else if (strcmp(cmd, "in") == 0) {
        // Set the GPIO as input.
        rpi_gpio_set_select(entry->gpio, 0);
    } else if (strcmp(cmd, "on") == 0) {
        if (rpi_gpio_get_select(entry->gpio) & 1) {
            rpi_gpio_set(entry->gpio);
        } else {
            return ENXIO;
        }
    } else if (strcmp(cmd, "off") == 0) {
        if (rpi_gpio_get_select(entry->gpio) & 1) {
            rpi_gpio_clear(entry->gpio);
        } else {
            return ENXIO;
        }
    } else if (strcmp(cmd, "pwm") == 0) {
        extern void pwm_debug(unsigned);
        pwm_debug(entry->gpio);
    } else {
        fprintf(stderr, "Unknown command '%s'\n", cmd);
    }

    // Reply to the caller.
    _IO_SET_WRITE_NBYTES(ctp, ncmdbytes);
    return EOK;
}

/**
 * Handles an _IO_MSG message.
 * This message allows a client to control any GPIO pin, using the various
 * RPI_GPIO_* subtypes.
 * This message can only be sent to the 'msg' node.
 * @param   ctp     Message context
 * @param   msg     rpi_gpio_msg_t message
 * @param   ocb     Control block for the open file
 * @return  For input messages, EOK if successful, error code otherwise
 *          For output messages, _RESMGR_PTR if successful, error code otherwise
 */
static int
msg_gpio(resmgr_context_t *ctp, io_msg_t *msg, iofunc_ocb_t *ocb)
{
    // Validate message/
    if (msg->i.mgrid != RPI_GPIO_IOMGR) {
        return EBADMSG;
    }

    // Do not allow combine messages.
    if (ctp->offset != 0) {
        return EBADMSG;
    }

	// Make sure we have a complete message.
    // Note that the frameworks guarantees that at list sizeof(io_msg_t) is
    // available.
    switch (msg->i.subtype) {
    case RPI_GPIO_ADD_EVENT:
        if (ctp->size < sizeof(rpi_gpio_event_t)) {
            return EBADMSG;
        }
        break;

    case RPI_GPIO_PWM_SETUP:
        if (ctp->size < sizeof(rpi_gpio_pwm_t)) {
            return EBADMSG;
        }
        break;
    case RPI_GPIO_SPI_INIT:
    case RPI_GPIO_SPI_WRITE_READ:
        if (ctp->size < sizeof(rpi_gpio_spi_t)) {
            return EBADMSG;
        }
        break;
    default:
        if (ctp->size < sizeof(rpi_gpio_msg_t)) {
            return EBADMSG;
        }
        break;
    }

    rpi_gpio_msg_t * const  rmsg = (void *)&msg->i;

    if (verbose) {
        printf("msg_gpio: type=%u gpio=%u value=%u\n",
               rmsg->hdr.subtype, rmsg->gpio, rmsg->value);
    }

    if (rmsg->gpio >= RPI_GPIO_NUM) {
        return ERANGE;
    }

    gpio_entry_t    *entry = (gpio_entry_t *)ocb->attr;
    if (entry->gpio != -1) {
        // Can only send this message to the 'msg' node.
        return ENXIO;
    }

    int rc = EOK;

    // Act on the various message subtypes.
    switch (rmsg->hdr.subtype) {
    case RPI_GPIO_SET_SELECT:
        if (rmsg->value > 7) {
            return ERANGE;
        }

        unsigned    prev = rpi_gpio_get_select(rmsg->gpio);
        rpi_gpio_set_select(rmsg->gpio, rmsg->value);
        rmsg->value = prev;
        rc = _RESMGR_PTR(ctp, rmsg, sizeof(*rmsg));
        break;
    case RPI_GPIO_GET_SELECT:
        rmsg->value = rpi_gpio_get_select(rmsg->gpio);
        rc = _RESMGR_PTR(ctp, rmsg, sizeof(*rmsg));
        break;
    case RPI_GPIO_WRITE:
        if (rpi_gpio_get_select(rmsg->gpio) == 1) {
            if (rmsg->value) {
                rpi_gpio_set(rmsg->gpio);
            } else {
                rpi_gpio_clear(rmsg->gpio);
            }
        } else {
            return ENXIO;
        }
        break;
    case RPI_GPIO_READ:
        if ((rpi_gpio_get_select(rmsg->gpio) & 1) == 0) {
            rmsg->value = rpi_gpio_read(rmsg->gpio);
            rc = _RESMGR_PTR(ctp, rmsg, sizeof(*rmsg));
        } else {
            return ENXIO;
        }
        break;
    case RPI_GPIO_ADD_EVENT:
        rc = event_add(ctp->rcvid, (rpi_gpio_event_t *)msg);
        break;
    case RPI_GPIO_PWM_SETUP:
        rc = pwm_setup(ctp->rcvid, (void *)msg);
        break;
    case RPI_GPIO_PWM_DUTY:
        rc = pwm_set_duty_cycle(ctp->rcvid, rmsg->gpio, rmsg->value);
        break;
    case RPI_GPIO_PUD:
        if (base_paddr == 0xfe000000) {
            if (!rpi_gpio_set_pud_bcm2711(rmsg->gpio, rmsg->value)) {
                rc = EINVAL;
            }
        } else {
            if (!rpi_gpio_set_pud_bcm2835(rmsg->gpio, rmsg->value)) {
                rc = EINVAL;
            }
        }
        break;
    case RPI_GPIO_SPI_INIT:
        rc = spi_init((void *)rmsg);
        break;
    case RPI_GPIO_SPI_WRITE_READ:
    {
        unsigned    replylen;
        rc = spi_write_read((void *)rmsg, ctp->info.msglen,
                            ctp->info.dstmsglen, &replylen);
        if ((rc == EOK) && (replylen > 0)) {
            MsgReply(ctp->rcvid, 0, rmsg, sizeof(rpi_gpio_spi_t) + replylen);
            return _RESMGR_NOREPLY;
        }
        break;
    }
    default:
        return EINVAL;
    }
    return rc;
}

/**
 * Clean up when a process closes a file descriptor to the resource manager.
 */
static int
close_gpio(resmgr_context_t *ctp, void *reserved, iofunc_ocb_t *ocb)
{
    event_remove_rcvid(ctp->rcvid);
    pwm_remove_rcvid(ctp->rcvid);
    return 0;
}

/**
 * Switch the run-time profile to reduce the privilege level of the driver.
 * @param   user_str    Optional user/group ID string passed on the command line
 * @return  true if successful, false otherwise
 */
static bool
drop_privileges(char const * const user_str)
{
    secpol_transition_type(NULL, NULL, 0);

    // Switch UID/GID.
    if (user_str != NULL) {
        if (set_ids_from_arg(user_str) == -1) {
            perror("set_ids_from_arg");
            return false;
        }

        if (verbose) {
            printf("Switched to uid %u gid %u (%s)\n", getuid(), getgid(),
                   user_str);
        }
    }

    return true;
}

/**
 * Main function.
 * Initializes the resource manager and then runs the message loop.
 */
int
main(int argc, char **argv)
{
    char const *user_str = NULL;
    mode_t      mode = 0755;
    const char  *mount = "/dev/gpio";
    unsigned    priority = 200;
    int         intr = 145;

    // Parse command-line options.
    for (;;) {
        int opt = getopt(argc, argv, ":a:i:m:o:p:s:u:v");
        if (opt == -1) {
            break;
        } else if (opt == 'a') {
            base_paddr = strtoul(optarg, NULL, 0);
        } else if (opt == 'i') {
            intr = strtoul(optarg, NULL, 0);
        } else if (opt == 'm') {
            mount = optarg;
        } else if (opt == 'o') {
            mode = strtoul(optarg, NULL, 0);
        } else if (opt == 'p') {
            priority = strtoul(optarg, NULL, 0);
        } else if (opt == 's') {
            shm_path = optarg;
        } else if (opt == 'u') {
            user_str = optarg;
        } else if (opt == 'v') {
            verbose++;
        } else {
            fprintf(stderr, "Unknown option: '%c\n", opt);
            return 1;
        }
    }

    // Map the GPIO registers.
    if (!rpi_gpio_map_regs(base_paddr)) {
        perror("Failed to map GPIOs");
        return 0;
    }

    if (!event_init(priority, intr)) {
        return 1;
    }

    if (!pwm_init()) {
        return 1;
    }

    // Initialize the GPIO nodes.
    for (unsigned i = 0; i < RPI_GPIO_NUM; i++) {
        gpio_entries[i].gpio = i;
        snprintf(gpio_entries[i].name, 8, "%u", i);
        iofunc_attr_init(&gpio_entries[i].attr, S_IFREG | 0660, NULL, NULL);
    }

    // Initialize the message node.
    gpio_entries[RPI_GPIO_NUM].gpio = -1;
    snprintf(gpio_entries[RPI_GPIO_NUM].name, 8, "msg");
    iofunc_attr_init(&gpio_entries[RPI_GPIO_NUM].attr, S_IFREG | 0660, NULL,
                     NULL);

    // Set up callback functions.
    iofunc_func_init(_RESMGR_CONNECT_NFUNCS, &connect_funcs,
                     _RESMGR_IO_NFUNCS, &io_funcs);

    connect_funcs.open = open_gpio;
    io_funcs.read = read_gpio;
    io_funcs.write = write_gpio;
    io_funcs.msg = msg_gpio;
    io_funcs.close_ocb = close_gpio;

    // Initialize directory attributes.
    iofunc_attr_init(&io_attr, S_IFDIR | mode, 0, 0);

    // Create the main dispatch structure.
    dispatch_t  *dispatch = dispatch_create();
    if (dispatch == NULL) {
        perror("dispatch_create");
        return 1;
    }

    // Attach to the mount point.
    int id = resmgr_attach(dispatch, NULL, mount, _FTYPE_ANY, _RESMGR_FLAG_DIR,
                           &connect_funcs, &io_funcs, &io_attr);
    if (id < 0) {
        perror("resmgr_attach");
        return 1;
    }

    if (!drop_privileges(user_str)) {
        return EXIT_FAILURE;
    }

    // Update all nodes to reflect the final uid/gid of the resource manager.
    uid_t const uid = geteuid();
    gid_t const gid = getegid();
    io_attr.uid = uid;
    io_attr.gid = gid;

    for (unsigned i = 0; i <= RPI_GPIO_NUM; i++) {
        gpio_entries[i].attr.uid = uid;
        gpio_entries[i].attr.gid = gid;
    }

    // Create the message context.
    dispatch_context_t  *ctx = dispatch_context_alloc(dispatch);
    if (ctx == NULL) {
        perror("dispatch_context_alloc");
        return 1;
    }

    // Message loop.
    for (;;) {
        ctx = dispatch_block(ctx);
        if (ctx == NULL) {
            perror("dispatch_block");
            return 1;
        }

        dispatch_handler(ctx);
    }

    return 0;
}
