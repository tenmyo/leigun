typedef struct Editor Editor;
/*
 *********************************************************************
 * Ed_EchoProc
 * 	The User has to register a proc for the commandline editor to 
 * 	write its echo of chars and control sequences.
 *********************************************************************
 */
typedef void Ed_EchoProc(void *clientData, void *buf, int len);
/*
 **************************************************************
 * Ed_LineSink
 * 	User proc where the commandline 
 * 	editor will sink a line as soon as it is completed by
 * 	pressing enter.
 **************************************************************
 */
typedef void Ed_LineSink(void *clientData, void *buf, int len);

/*
 ********************************************************************
 * Editor_New
 *	Constructor for a editor. A echo destination and a 
 *	destination for completed lines is required.
 ********************************************************************
 */
Editor *Editor_New(Ed_EchoProc *, Ed_LineSink *, void *clientData);
/*
 *****************************************************************
 * Editor_Del
 *	The destructor of an editor
 *****************************************************************
 */
void Editor_Del(Editor * ed);
/*
 *********************************************************************
 * Editor_Feed
 *	The interface for incoming characters 
 *	(For example a telnetd or a terminal can feed the editor)
 *********************************************************************
 */
int Editor_Feed(Editor * ed, char c);
