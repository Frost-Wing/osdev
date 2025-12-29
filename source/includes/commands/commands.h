/**
 * @file commands.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The almost fully replicated linux basic commands of sh.
 * @version 0.1
 * @date 2025-10-07
 * 
 * @copyright Copyright (c) Pradosh 2025
 * 
 */
#ifndef COMMANDS_H
#define COMMANDS_H

#pragma once
#include <basics.h>
#include <graphics.h>

extern struct fwrfs* global_fs;

int cmd_echo(int argc, char** argv);
int cmd_touch(int argc, char** argv);
int cmd_rm(int argc, char** argv);
int cmd_mkdir(int argc, char** argv);
int cmd_cat(int argc, char** argv);
int cmd_ls(int argc, char** argv);
int cmd_pwd(int argc, char** argv);
int cmd_cd(int argc, char** argv);
int cmd_whoami(int argc, char** argv);
int cmd_shutdown(int argc, char** argv);
int cmd_reboot(int argc, char** argv);
int cmd_fwfetch(int argc, char** argv);
int cmd_help(int argc, char** argv);
int cmd_lspci(int argc, char** argv);
int cmd_clear(int argc, char** argv);
int cmd_lsblk(int argc, char** argv);

#endif