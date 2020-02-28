//
// mlu_gen.txt - generator of MLU files for the ALPHA-g TPC trigger
//
// K.Olchanski/TRIUMF
//

#include <stdio.h>
#include <stdlib.h> // exit()
#include <assert.h> // assert()

const int bits = 16;
const int size = (1<<bits);

int wrap(int i)
{
   assert(i>=0);
   while (i>=bits)
      i-=bits;
   assert(i>=0);
   return i;
}

int bit(int i)
{
   return 1<<wrap(i);
}

int count_bits(int b)
{
   int c = 0;
   for (int i=0; i<bits; i++) {
      if (b & (1<<i))
         c++;
   }
   return c;
}

int count_clusters(int b)
{
   int c = 0;
   int h = 0;
   for (int i=0; i<bits; i++) {
      if (b & (1<<i)) {
         if (!h) {
            if ((i==0) && (b & bit(size-1))) {
               // cluster wraparound
            } else {
               c++;
            }
         }
         h = 1;
      } else {
         h = 0;
      }
   }
   return c;
}

void print_bits(FILE *fp, int b)
{
   for (int i=0; i<bits; i++) {
      if (b & (1<<i))
         fprintf(fp, "X");
      else
         fprintf(fp, ".");
   }
}

class MLU
{
public:
   int mlu[size];
   
public:
   MLU() // ctor
   {
      for (int i=0; i<size; i++) {
         mlu[i] = -1;
      }
   }

   void Write(const char* filename)
   {
      int count = 0;
      FILE *fp = fopen(filename, "w");

      for (int i=0; i<size; i++) {
         if (mlu[i] > 0) {
            fprintf(fp, "0x%04x %d ", i, mlu[i]);
            print_bits(fp, i);
            fprintf(fp, ", %d bits, %d clusters\n", count_bits(i), count_clusters(i));
            count ++;
         }
      }

      fclose(fp);

      printf("Wrote %d bits to %s\n", count, filename);
   }

   void PrintUndefined()
   {
      int count = 0;
      printf("MLU:\n");
      for (int i=0; i<size; i++) {
         if (mlu[i] < 0) {
            printf("bits 0x%04x undefined: ", i);
            print_bits(stdout, i);
            printf(", %d bits, %d clusters\n", count_bits(i), count_clusters(i));
            //exit(1);
            count++;
         }
      }
      printf("Undefined count: %d\n", count);
   }
};

int main(int argc, char* argv[])
{
   int selected = 0;

   for (int i=0; i<argc; i++) {
      printf("argv[%d] is [%s]\n", i, argv[i]);
   }

   if (argc == 2) {
      selected = atoi(argv[1]);
   }

   if (argc != 2) {
      printf("Please select the MLU configuration:\n");
      printf(" mlu_gen 0 --- empty MLU, do not trigger on anything\n");
      printf(" mlu_gen 1 --- 1-or-more MLU, trigger on anything\n");
      printf(" mlu_gen 2 --- 2-or-more MLU, trigger on 2 or more hit clusters\n");
      printf(" mlu_gen 3 --- 3-or-more MLU, trigger on 3 or more hit clusters\n");
      printf(" mlu_gen 4 --- 4-or-more MLU, trigger on 4 or more hit clusters\n");
      exit(1);
   }

   printf("selected MLU configuration: %d\n", selected);

   if (selected == 0) {
      printf("selected empty MLU file!\n");
      MLU mlu;
      mlu.Write("mlu_empty.txt");
   } else if (selected == 1) {
      printf("selected 1-or-more MLU file!\n");
      MLU mlu;
      mlu.mlu[0] = 0;
      for (int i=1; i<size; i++) {
         mlu.mlu[i] = 1;
      }
      mlu.Write("mlu_1ormore.txt");
   } else if (selected == 2) {
      printf("selected 2-or-more MLU file!\n");
      MLU mlu;
      mlu.mlu[0] = 0;

      // do not trigger if everything is on

      mlu.mlu[0xFFFF] = 0;

      // 1 bit
      
      for (int i=0; i<size; i++) {
         if (mlu.mlu[i] < 0) {
            if (count_bits(i) == 1)
               mlu.mlu[i] = 0;
         }
      }
      
      // 1 cluster
      
      for (int i=0; i<size; i++) {
         if (mlu.mlu[i] < 0) {
            if (count_clusters(i) == 1)
               mlu.mlu[i] = 0;
         }
      }

      // trigger on anything left over
      for (int i=0; i<size; i++) {
         if (mlu.mlu[i] < 0) {
            mlu.mlu[i] = 1;
         }
      }

      mlu.Write("mlu_2ormore.txt");
   } else if (selected == 22) {
      printf("selected 2-or-more with gap 2-or-more MLU file!\n");
      MLU mlu;
      mlu.mlu[0] = 0;

      // do not trigger if everything is on

      mlu.mlu[0xFFFF] = 0;

      // 1 bit
      
      for (int i=0; i<size; i++) {
         if (mlu.mlu[i] < 0) {
            if (count_bits(i) == 1)
               mlu.mlu[i] = 0;
         }
      }
      
      // 1 cluster
      
      for (int i=0; i<size; i++) {
         if (mlu.mlu[i] < 0) {
            if (count_clusters(i) == 1)
               mlu.mlu[i] = 0;
         }
      }

      // veto anything with 2 clusters too close together

      for (int i=0; i<size; i++) {
         for (int b=0; b<bits; b++) {
            if ((i&bit(b))&&(!(i&bit(b+1)))&&(i&bit(b+2))) {
               mlu.mlu[i] = 0;
               if (0) {
                  printf("kill 0x%04x - ", i);
                  print_bits(stdout, i);
                  printf("\n");
               }
            }
         }
      }

      // trigger on anything left over
      for (int i=0; i<size; i++) {
         if (mlu.mlu[i] < 0) {
            mlu.mlu[i] = 1;
         }
      }

      mlu.Write("mlu_2ormore_gap_2ormore.txt");
   } else if (selected == 3) {
      printf("selected 3-or-more MLU file!\n");
      MLU mlu;
      mlu.mlu[0] = 0;

      // do not trigger if everything is on

      mlu.mlu[0xFFFF] = 0;

      // veto 1 bit
      
      for (int i=0; i<size; i++) {
         if (mlu.mlu[i] < 0) {
            if (count_bits(i) == 1)
               mlu.mlu[i] = 0;
         }
      }
      
      // veto 1 and 2 cluster
      
      for (int i=0; i<size; i++) {
         if (mlu.mlu[i] < 0) {
            if (count_clusters(i) <= 2)
               mlu.mlu[i] = 0;
         }
      }

      // trigger on anything left over

      for (int i=0; i<size; i++) {
         if (mlu.mlu[i] < 0) {
            mlu.mlu[i] = 1;
         }
      }

      mlu.Write("mlu_3ormore.txt");
   } else if (selected == 4) {
      printf("selected 4-or-more MLU file!\n");
      MLU mlu;
      mlu.mlu[0] = 0;

      // do not trigger if everything is on

      mlu.mlu[0xFFFF] = 0;

      // veto 1 bit
      
      for (int i=0; i<size; i++) {
         if (mlu.mlu[i] < 0) {
            if (count_bits(i) == 1)
               mlu.mlu[i] = 0;
         }
      }
      
      // veto 1 and 2 cluster
      
      for (int i=0; i<size; i++) {
         if (mlu.mlu[i] < 0) {
            if (count_clusters(i) <= 3)
               mlu.mlu[i] = 0;
         }
      }

      // trigger on anything left over

      for (int i=0; i<size; i++) {
         if (mlu.mlu[i] < 0) {
            mlu.mlu[i] = 1;
         }
      }

      mlu.Write("mlu_4ormore.txt");
   }

#if 0
   // 0 clusters
   
   if (1) { // 0 bits
      mlu[0] = 0;
   }

   // 0 clusters
   
   if (1) { // everything is on
      mlu[0xFFFF] = 0;
   }

   // 1 bit

   for (int i=0; i<size; i++) {
      if (mlu[i] < 0) {
         if (count_bits(i) == 1)
            mlu[i] = 0;
      }
   }

   // 1 cluster

   for (int i=0; i<size; i++) {
      if (mlu[i] < 0) {
         if (count_clusters(i) == 1)
            mlu[i] = 0;
      }
   }

#if 0
   // 1 clusters
   
   if (1) { // 1 bits
      for (int i=0; i<bits; i++) {
         mlu[1<<i] = 0;
      }
   }
   
   if (1) { // 2 adjacent bits
      for (int i=0; i<bits; i++) {
         int k = (1<<i) | (1<<wrap(i+1));
         mlu[k] = 0;
         //printf("bits 0x%04x\n", k);
      }
   }
   
   if (1) { // 3 adjacent bits
      for (int i=0; i<bits; i++) {
         int k = (1<<i) | (1<<wrap(i+1)) | (1<<wrap(i+2));
         mlu[k] = 0;
         //printf("bits 0x%04x\n", k);
      }
   }
   
   if (1) { // 4 adjacent bits
      for (int i=0; i<bits; i++) {
         int k = bit(i) | bit(i+1) | bit(i+2) | bit(i+3);
         mlu[k] = 0;
         //printf("bits 0x%04x\n", k);
      }
   }

   if (1) { // 5 adjacent bits
      for (int i=0; i<bits; i++) {
         int k = bit(i) | bit(i+1) | bit(i+2) | bit(i+3) | bit(i+4);
         mlu[k] = 0;
         //printf("bits 0x%04x\n", k);
      }
   }

   // 2 clusters, gap 1
   
   if (1) { // 2 bits, gap 1 bit
      for (int i=0; i<bits; i++) {
         int k = (1<<i) | (1<<wrap(i+2));
         mlu[k] = 0;
         //printf("bits 0x%04x\n", k);
      }
   }
   
   if (1) { // 2 bits + 1 bits, gap 1 bits
      for (int i=0; i<bits; i++) {
         int k = (1<<i) | (1<<wrap(i+1)) | 1<<wrap(i+3);
         mlu[k] = 0;
         //printf("bits 0x%04x\n", k);
      }
   }
   
   if (1) { // 1 bits + 2 bits, gap 1 bits
      for (int i=0; i<bits; i++) {
         int k = (1<<i) | (1<<wrap(i+2)) | 1<<wrap(i+3);
         mlu[k] = 0;
         //printf("bits 0x%04x\n", k);
      }
   }

   if (1) { // XXX.X...........
      for (int i=0; i<bits; i++) {
         int k = bit(i) | bit(i+1) | bit(i+2) | bit(i+4);
         mlu[k] = 0;
         //printf("bits 0x%04x\n", k);
      }
   }

   if (1) { // XX.XX...........
      for (int i=0; i<bits; i++) {
         int k = bit(i) | bit(i+1) | bit(i+3) | bit(i+4);
         mlu[k] = 0;
         //printf("bits 0x%04x\n", k);
      }
   }

   if (1) { // X.XXX...........
      for (int i=0; i<bits; i++) {
         int k = bit(i) | bit(i+2) | bit(i+3) | bit(i+4);
         mlu[k] = 0;
         //printf("bits 0x%04x\n", k);
      }
   }

   // 3 clusters, gap 1
      
   if (1) { // 3 bits, gap 1 bits, X.X.X...........
      for (int i=0; i<bits; i++) {
         int k = bit(i) | bit(i+2) | bit(i+4);
         mlu[k] = 0;
         //printf("bits 0x%04x\n", k);
      }
   }

   // 2 clusters, gap 2+
      
   if (1) { // 2 bits, gap 2 bits
      for (int i=0; i<bits; i++) {
         int k = (1<<i) | (1<<wrap(i+3));
         mlu[k] = 1;
         //printf("bits 0x%04x\n", k);
      }
   }
   
   if (1) { // 2 bits, gap 3 bits
      for (int i=0; i<bits; i++) {
         int k = bit(i) | bit(i+4);
         mlu[k] = 1;
         //printf("bits 0x%04x\n", k);
      }
   }
   
   if (1) { // X....X.......... 2 bits, gap 4 bits
      for (int i=0; i<bits; i++) {
         int k = bit(i) | bit(i+5);
         mlu[k] = 1;
         //printf("bits 0x%04x\n", k);
      }
   }
   
   if (1) { // 2 bits, gap 5 bits
      for (int i=0; i<bits; i++) {
         int k = bit(i) | bit(i+6);
         mlu[k] = 1;
         //printf("bits 0x%04x\n", k);
      }
   }
   
   if (1) { // 2 bits, gap 6 bits
      for (int i=0; i<bits; i++) {
         int k = bit(i) | bit(i+7);
         mlu[k] = 1;
         //printf("bits 0x%04x\n", k);
      }
   }
   
   if (1) { // 2 bits, gap 7 bits
      for (int i=0; i<bits; i++) {
         int k = bit(i) | bit(i+8);
         mlu[k] = 1;
         //printf("bits 0x%04x\n", k);
      }
   }
   
   if (1) { // 2 bits + 1 bits, gap 2 bits
      for (int i=0; i<bits; i++) {
         int k = bit(i) | bit(i+1) | bit(i+4);
         mlu[k] = 1;
         //printf("bits 0x%04x\n", k);
      }
   }
   
   if (1) { // XX...X.......... 2 bits + 1 bits, gap 3 bits
      for (int i=0; i<bits; i++) {
         int k = bit(i) | bit(i+1) | bit(i+5);
         mlu[k] = 1;
         //printf("bits 0x%04x\n", k);
      }
   }
   
   if (1) { // 2 bits + 1 bits, gap 4 bits
      for (int i=0; i<bits; i++) {
         int k = bit(i) | bit(i+1) | bit(i+6);
         mlu[k] = 1;
         //printf("bits 0x%04x\n", k);
      }
   }
   
   if (1) { // 2 bits + 1 bits, gap 5 bits
      for (int i=0; i<bits; i++) {
         int k = bit(i) | bit(i+1) | bit(i+7);
         mlu[k] = 1;
         //printf("bits 0x%04x\n", k);
      }
   }
   
   if (1) { // 2 bits + 1 bits, gap 6 bits
      for (int i=0; i<bits; i++) {
         int k = bit(i) | bit(i+1) | bit(i+8);
         mlu[k] = 1;
         //printf("bits 0x%04x\n", k);
      }
   }
   
   if (1) { // X..XX..........., 1 bits + 2 bits, gap 2 bits
      for (int i=0; i<bits; i++) {
         int k = bit(i) | bit(i+3) | bit(i+4);
         mlu[k] = 1;
         //printf("bits 0x%04x\n", k);
      }
   }
   
   if (1) { // 1 bits + 2 bits, gap 3 bits
      for (int i=0; i<bits; i++) {
         int k = bit(i) | bit(i+4) | bit(i+5);
         mlu[k] = 1;
         //printf("bits 0x%04x\n", k);
      }
   }
   
   if (1) { // 1 bits + 2 bits, gap 4 bits
      for (int i=0; i<bits; i++) {
         int k = bit(i) | bit(i+5) | bit(i+6);
         mlu[k] = 1;
         //printf("bits 0x%04x\n", k);
      }
   }
   
   if (1) { // 1 bits + 2 bits, gap 5 bits
      for (int i=0; i<bits; i++) {
         int k = bit(i) | bit(i+6) | bit(i+7);
         mlu[k] = 1;
         //printf("bits 0x%04x\n", k);
      }
   }
   
   if (1) { // 1 bits + 2 bits, gap 6 bits
      for (int i=0; i<bits; i++) {
         int k = bit(i) | bit(i+7) | bit(i+8);
         mlu[k] = 1;
         //printf("bits 0x%04x\n", k);
      }
   }

   // high multiplicity

   for (int i=0; i<size; i++) {
      if (mlu[i] < 0) {
         if (count_bits(i) > 8)
            mlu[i] = 1;
      }
   }
#endif

   if (1) { // trigger on anything left over
      for (int i=0; i<size; i++) {
         if (mlu[i] < 0) {
            mlu[i] = 1;
         }
      }
   }
   
   int count = 0;
   printf("MLU:\n");
   for (int i=0; i<size; i++) {
      if (mlu[i] < 0) {
         printf("bits 0x%04x undefined: ", i);
         print_bits(stdout, i);
         printf(", %d bits, %d clusters\n", count_bits(i), count_clusters(i));
         //exit(1);
         count++;
      }
   }
   printf("Undefined count: %d\n", count);

   FILE *fp = fopen("test.mlu", "w");

   for (int i=0; i<size; i++) {
      if (mlu[i] > 0) {
         fprintf(fp, "0x%04x %d ", i, mlu[i]);
         print_bits(fp, i);
         fprintf(fp, ", %d bits, %d clusters\n", count_bits(i), count_clusters(i));
      }
   }

   fclose(fp);
#endif

   return 0;
}

/* emacs
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 3
 * indent-tabs-mode: nil
 * End:
 */
