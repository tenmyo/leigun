const char *gdb_arm_features = "<?xml version=\"1.0\"?>\n"
"<!-- Copyright (C) 2007, 2008, 2009, 2010 Free Software Foundation, Inc.\n"
"\n"
"     Copying and distribution of this file, with or without modification,\n"
"     are permitted in any medium without royalty provided the copyright\n"
"     notice and this notice are preserved.  -->\n"
"\n"
"<!DOCTYPE feature SYSTEM \"gdb-target.dtd\">\n"
"<feature name=\"org.gnu.gdb.arm.core\">\n"
"  <reg name=\"r0\" bitsize=\"32\"/>\n"
"  <reg name=\"r1\" bitsize=\"32\"/>\n"
"  <reg name=\"r2\" bitsize=\"32\"/>\n"
"  <reg name=\"r3\" bitsize=\"32\"/>\n"
"  <reg name=\"r4\" bitsize=\"32\"/>\n"
"  <reg name=\"r5\" bitsize=\"32\"/>\n"
"  <reg name=\"r6\" bitsize=\"32\"/>\n"
"  <reg name=\"r7\" bitsize=\"32\"/>\n"
"  <reg name=\"r8\" bitsize=\"32\"/>\n"
"  <reg name=\"r9\" bitsize=\"32\"/>\n"
"  <reg name=\"r10\" bitsize=\"32\"/>\n"
"  <reg name=\"r11\" bitsize=\"32\"/>\n"
"  <reg name=\"r12\" bitsize=\"32\"/>\n"
"  <reg name=\"sp\" bitsize=\"32\" type=\"data_ptr\"/>\n"
"  <reg name=\"lr\" bitsize=\"32\"/>\n"
"  <reg name=\"pc\" bitsize=\"32\" type=\"code_ptr\"/>\n"
"\n"
"  <!-- The CPSR is register 25, rather than register 16, because\n"
"       the FPA registers historically were placed between the PC\n"
"       and the CPSR in the \"g\" packet.  -->\n"
"  <reg name=\"cpsr\" bitsize=\"32\" regnum=\"25\"/>\n"
"</feature>\n";
