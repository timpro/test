
#include <stdio.h>
#include <stdint.h>

int main()
{
	uint32_t i, j, lsb, lfsr, outbyte;
	char *instring = "$$UBSEDS22G,00:00:00,0.0000,0.0000 $UBSEDS22G,00:00:00,0.0000,0.0000 $UBSEDS22G,00:00:00,0.0000,0.0000  \0\0\0";
	char scrambled[260];
	uint16_t interleaved[260];

	printf("%s\n", instring);

	// apply scrambler
	lfsr = 0;
	for (i = 0; instring[i] > 0; i++) {
		outbyte = 0;
		for (j = 0; j < 8; j++ ) {
			lsb = (instring[i] >> j);
			lsb ^= (lfsr >> 12);
			lsb ^= (lfsr >> 17);
			lsb &= 0b1;
			lfsr <<= 1;
			lfsr |= lsb;
			outbyte |= lsb << j;
		}
		scrambled[i] = outbyte;
		printf("%c%c", 65 + (outbyte & 15), 65 + ((outbyte >> 4) & 15));
	}
	scrambled[i] = 0;
	printf("\n");

	// replace 1x bytes of data with 2x bytes of FEC and interleave
	lfsr = 0;
	for (i = 0; instring[i] > 0; i++) {
		uint16_t g1, g2, w[4], outword;
		outword = 0;
		for (j = 0; j < 8; j++ ) {
			lfsr <<= 1;
			lfsr |= (scrambled[i] >> j) & 0b1;
			g1 = 0b1 & ( lfsr ^ (lfsr >> 3) ^ (lfsr >> 4) );
			g2 = 0b1 & ( lfsr ^ (lfsr >> 1) ^ (lfsr >> 2)  ^ (lfsr >> 4) );
			outword |= g1 << (j + j);
			outword |= g2 << (j + j + 1);
		}
		for (j = 0; j < 4; j++ ) {
			w[j]  = 0x1 & (outword >> (0 + j - 0));
			w[j] |= 0x2 & (outword >> (4 + j - 1));
			w[j] |= 0x4 & (outword >> (8 + j - 2));
			w[j] |= 0x8 & (outword >> (12+ j - 3));
		}
		outword = w[0] | (w[1] << 4) | (w[2] << 8) | (w[3] << 12);
		interleaved[i] = outword;
//		printf("%c%c", 65 + ((outword >> 0) & 15), 65 + ((outword >> 4) & 15));
//		printf("%c%c", 65 + ((outword >> 8) & 15), 65 + ((outword >> 12) & 15));
	}
//	printf("\n");

#if 1
	// puncture data
	for (i = 0; instring[i] > 0; i++) {
		uint16_t outword;
		outword = interleaved[i] ^ (1 <<((5*i) & 15)) ;
		interleaved[i] = outword;
//		printf("%c%c", 65 + ((outword >> 0) & 15), 65 + ((outword >> 4) & 15));
//		printf("%c%c", 65 + ((outword >> 8) & 15), 65 + ((outword >> 12) & 15));
	}
//	printf("\n");
#endif

	// interleaving is self-inverse
	for (i = 0; instring[i] > 0; i++) {
		uint16_t w[4], outword;
		outword = interleaved[i];
		for (j = 0; j < 4; j++ ) {
			w[j]  = 0x1 & (outword >> (0 + j - 0));
			w[j] |= 0x2 & (outword >> (4 + j - 1));
			w[j] |= 0x4 & (outword >> (8 + j - 2));
			w[j] |= 0x8 & (outword >> (12+ j - 3));
		}
		outword = w[0] | (w[1] << 4) | (w[2] << 8) | (w[3] << 12);
		interleaved[i] = outword;
	}

	// TODO: viterbi
	// First pass, use 12 bits of random
	uint16_t best, least, test, last, lastk, errcount;
	last = lfsr = errcount = 0;
	for (i = 0; instring[i] > 0; i++) {
		least = 15;
		best = 0;
		for (j = 0; least && (j < 4096); j++ ) {
			uint16_t k, g1, g2, outword;
			outword = 0;
			for (k = 0; k < 4; k++ ) {
				lfsr <<= 1;
				lfsr |= (j >> (8 + k)) & 0b1;
			}
			for (k = 0; k < 8; k++ ) {
				lfsr <<= 1;
				lfsr |= (j >> k) & 0b1;
				g1 = 0b1 & ( lfsr ^ (lfsr >> 3) ^ (lfsr >> 4) );
				g2 = 0b1 & ( lfsr ^ (lfsr >> 1) ^ (lfsr >> 2)  ^ (lfsr >> 4) );
				outword |= g1 << (k + k);
				outword |= g2 << (k + k + 1);
			}
			test = interleaved[i] ^ outword;
			for (k = 0; test; k++ )
				test &= test - 1;
			if (k == least) {
				// resolve collision
				test = 0x0f & ((j >> 8) ^ last);
				for (k = 0; test; k++ )
					test &= test - 1;
				if (k < lastk)
					best = j;
			} else if (k < least) {
				least = k;
				test = 0x0f & ((j >> 8) ^ last);
				for (k = 0; test; k++ )
					test &= test - 1;
				lastk = k; // needed to resolve future collisions
				best = j;
			}
		}
		errcount += least;
		last = best >> 4;
		scrambled[i] = best;
		printf("%c%c", 65 + (best & 15), 65 + ((best >> 4) & 15));
	}
	printf("\nError count:%d\n", errcount);

	// reverse scrambler
	lfsr = 0;
	for (i = 0; instring[i] > 0; i++) {
		outbyte = 0;
		for (j = 0; j < 8; j++ ) {
			lsb = (scrambled[i] >> j);
			lsb ^= (lfsr >> 12);
			lsb ^= (lfsr >> 17);
			lsb &= 0b1;
			lfsr <<= 1;
			lfsr |= (scrambled[i] >> j) & 0b1;

			outbyte |= lsb << j;
		}
		scrambled[i] = outbyte;
	}
	scrambled[i] = 0;
	for (i = 0; instring[i] > 0; i++)
		if (scrambled[i] < 32 || scrambled[i] > 127)
			scrambled[i] = '.'; // ASCII sanity check
	printf("%s\n", scrambled);


	return 0;
}
