/**
 * @file commands.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The almost fully replicated linux basic commands of sh.
 * @version 0.1
 * @date 2025-10-07
 *
 * @copyright Copyright (c) Pradosh 2025-2026
 *
 */
#ifndef COMMANDS_H
#define COMMANDS_H

#include <basics.h>
#include <filesystems/vfs.h>
#include <graphics.h>

extern struct fwrfs *global_fs;

/** @brief Print the supplied arguments. */
int cmd_echo(int argc, char **argv);
/** @brief Create or update a file's timestamp. */
int cmd_touch(int argc, char **argv);
/** @brief Remove files or directories. */
int cmd_rm(int argc, char **argv);
/** @brief Create directories. */
int cmd_mkdir(int argc, char **argv);
/** @brief Print file contents. */
int cmd_cat(int argc, char **argv);
/** @brief List directory entries. */
int cmd_ls(int argc, char **argv);
/** @brief Print the current working directory. */
int cmd_pwd(int argc, char **argv);
/** @brief Change the current working directory. */
int cmd_cd(int argc, char **argv);
/** @brief Print the current user name. */
int cmd_whoami(int argc, char **argv);
/** @brief Shut down the system. */
int cmd_shutdown(int argc, char **argv);
/** @brief Reboot the system. */
int cmd_reboot(int argc, char **argv);
/** @brief Display FrostWing system information. */
int cmd_fwfetch(int argc, char **argv);
/** @brief Print available shell commands. */
int cmd_help(int argc, char **argv);
/** @brief List detected PCI devices. */
int cmd_lspci(int argc, char **argv);
/** @brief Clear the terminal display. */
int cmd_clear(int argc, char **argv);
/** @brief List block devices. */
int cmd_lsblk(int argc, char **argv);
/** @brief List detected USB devices. */
int cmd_lsusb(int argc, char **argv);
/** @brief Mount a filesystem. */
int cmd_mount(int argc, char **argv);
/** @brief Move or rename a file. */
int cmd_mv(int argc, char **argv);
/** @brief Unmount a filesystem. */
int cmd_umount(int argc, char **argv);
/** @brief Execute a program. */
int cmd_exec(int argc, char **argv);
/** @brief List running tasks. */
int cmd_tasks(int argc, char **argv);
/** @brief Send ICMP echo requests to a host. */
int cmd_ping(int argc, char **argv);
/** @brief Download a resource over HTTP. */
int cmd_wget(int argc, char **argv);
/** @brief Copy a file. */
int cmd_cp(int argc, char **argv);

#endif