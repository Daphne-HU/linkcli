/*
 * Add this stanza inside your SECTIONS { } block, typically after .rodata.
 *
 * The KEEP() prevents the linker from discarding the command descriptors
 * as unreferenced (they are only referenced via the section bounds symbols).
 */

.cli_cmds :
{
    __start_cli_cmds = .;
    KEEP(*(.cli_cmds))
    __stop_cli_cmds = .;
} > FLASH   /* or wherever your read-only data lives */
