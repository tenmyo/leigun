/*
 *************************************************************************************************
 *
 * Main for standalone PCL3GUI to image converter
 *
 * state: working
 *
 * Copyright 2006 Jochen Karrer. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright notice, this list
 *       of conditions and the following disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY Jochen Karrer ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those of the
 * authors and should not be interpreted as representing official policies, either expressed
 * or implied, of Jochen Karrer.
 *
 *************************************************************************************************
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "dj460interp.h"
int
main(int argc, char *argv[])
{
	Dj460Interp *interp;
	int fd;
	int count;
	uint8_t buf[4096];
	int result, result2;
	if (argc < 3) {
		fprintf(stderr, "Syntax:\n\t%s pclfile.pcl <outputdir>\n\n", argv[0]);
		exit(1);
	}
	fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		perror("can not open pcl file");
		exit(1);
	}

	interp = Dj460Interp_New(argv[2]);
	do {
		result = read(fd, buf, sizeof(buf));
		if (result < 0) {
			perror("error reading pcl file");
			exit(1);
		} else if (result == 0) {
			Dj460Interp_Reset(interp);
			exit(0);
		}
		for (count = 0; count < result;) {
			result2 = Dj460Interp_Feed(interp, buf, result);
			if (result2 <= 0) {
				fprintf(stderr, "Can not feed the interpreter\n");
				break;
			}
			count += result2;
		}
	} while (1);
	exit(0);
}
