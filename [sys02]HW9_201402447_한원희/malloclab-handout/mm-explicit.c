/*
 * mm-explicit.c - an empty malloc package
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 *
 * @id : 201402447 
 * @name : 한원희
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mm.h"
#include "memlib.h"

/* If you want debugging output, use the following macro.  When you hand
 * in, remove the #define DEBUG line. */
#define DEBUG
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
#else
# define dbg_printf(...)
#endif


/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif /* def DRIVER */

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(p) (((size_t)(p) + (ALIGNMENT-1)) & ~0x7)
#define WSIZE       4	 // word size선언
#define DSIZE       8	 // double word size 선언
#define CHUNKSIZE  16	 // 초기 heap size
#define MINIMUM    24   // 최소block size
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define PACK(size, alloc)  ((size) | (alloc)) // size와 allocated bit를 word에 삽입한다

#define GET(p)       (*(int *)(p)) 
#define PUT(p, val)  (*(int *)(p) = (val))
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)
#define HDRP(bp)       ((void *)(bp) - WSIZE) // header 반환
#define FTRP(bp)       ((void *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) // footer 반환
#define NEXT_BLKP(bp)  ((void *)(bp) + GET_SIZE(HDRP(bp))) // 다음 블럭의 주소 반환
#define PREV_BLKP(bp)  ((void *)(bp) - GET_SIZE(HDRP(bp) - WSIZE)) // 이전 블럭의 주소 반환
#define NEXT_FREEP(bp)(*(void **)(bp + DSIZE)) // 다음 free block의 주소 반환
#define PREV_FREEP(bp)(*(void **)(bp)) // 이전 free block의 주소 반환

static char *heap_listp = 0; 
static char *free_listp = 0;

static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void insertAtFront(void *bp); 
static void removeBlock(void *bp); 

/*
 * Initialize: return -1 on error, 0 on success.
 */
int mm_init(void) {
	// 초기 empty heap
 	if((heap_listp = mem_sbrk(2*MINIMUM)) == NULL) 
                return -1;
        PUT(heap_listp, 0); // padding

	// 처음 블록을 초기화해줍니다
        PUT(heap_listp + WSIZE, PACK(MINIMUM,1 )); 
        PUT(heap_listp + DSIZE, 0); 		// PREV pointer
        PUT(heap_listp + DSIZE+WSIZE, 0); 	//NEXT pointer

        PUT(heap_listp + MINIMUM, PACK(MINIMUM, 1)); // footer에 dummy값 삽입
        PUT(heap_listp + WSIZE + MINIMUM, PACK(0,1)); // tail에 dummy값 삽입

        free_listp = heap_listp + DSIZE; 
	// free_listp를 heap에 prev의 pointer를 가리키도록 함

        if((extend_heap(CHUNKSIZE/WSIZE)) == NULL) 	// heap을 확장함
                return -1;
        return 0;	
}

/*
 * malloc
 */
void *malloc (size_t size) {

        size_t asize; 		// block의 size를 저장
        size_t extendsize; 	// heap에 삽입할 공간이 없으면 확장할 공간을 정의한다 
        char *bp;

        if(size <= 0)
                return NULL;
	
	asize = MAX(ALIGN(size) + DSIZE, MINIMUM);

        if((bp = find_fit(asize))){	// fit을 할 free list를 탐색한다
                place(bp, asize);
                return bp;
        }
        // free list가 존재하지 않으면 block을 위치시키기 위한 메모리를 확장한다
        extendsize = MAX(asize, CHUNKSIZE);
        // heap을 확장할 수 없으면 NULL을 반환한다
        if((bp = extend_heap(extendsize/WSIZE)) == NULL)
                return NULL;
        place(bp, asize);
        return bp;

}

void mm_checkheap(int verbose) {
}

/*
 * free
 */
void free (void *ptr) {
        if(!ptr) return; // pointer가 NULL이라면 return 한다
        size_t size = GET_SIZE(HDRP(ptr));

        //heaer와 footer를 비할당으로 바꾼다
        PUT(HDRP(ptr), PACK(size, 0));
        PUT(FTRP(ptr), PACK(size, 0));
        coalesce(ptr); // bp를 free block으로 coalesce와 add한다
}
/*
 * realloc - you may want to look at mm-naive.c
 */
void *realloc(void *oldptr, size_t size){

  	size_t oldsize;//원래 크기를 담을 변수
     void *newptr;  //새로운 포인터 변수
     /* If size == 0 then this is just free, and we return NULL. */
     if(size == 0) {
          free(oldptr);
          return 0;
     }
     /* If oldptr is NULL, then this is just malloc. */
     if(oldptr == NULL) {
          return malloc(size);
     }
     newptr = malloc(size);
     /* If realloc() fails the original block is left untouched  */
     if(!newptr) {
          return 0;
     }
     /* Copy the old data. */
   //원래 할당받았던 블록의 데이터를 복사하고 새로 크기를 받아서 할당한다
     oldsize = GET_SIZE(oldptr);
     if(size < oldsize) oldsize = size;
    memcpy(newptr, oldptr, oldsize);
     /* Free the old block. */
   //원래 할당받았던 블록은 할당 해제한다
     free(oldptr);
     return newptr;	
}

/*
 * calloc - you may want to look at mm-naive.c
 * This function is not tested by mdriver, but it is
 * needed to run the traces.
 */
void *calloc (size_t nmemb, size_t size){
	return NULL;
}


static void *extend_heap(size_t words){

        char *bp;
        size_t size;

        size = (words % 2) ? (words+1) * WSIZE : words * WSIZE; //words가 홀수일 경우 words에 1을 더해 줌
        if(size < MINIMUM) // size가 24보다 작을때
                size = MINIMUM; // size = 24
        if((long)(bp =mem_sbrk(size))==-1) // size만큼 heap을 확장 해줌
                return NULL;


        PUT(HDRP(bp), PACK(size, 0)); //free block header에 dummy 삽입
        PUT(FTRP(bp), PACK(size, 0)); //free block footer에 dummy 삽입
        PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1)); // 새로운 epilogue header에 dummy삽입

        return coalesce(bp); // coalesce함수 호출 

}

static void place(void *bp, size_t asize){
        size_t csize = GET_SIZE(HDRP(bp)); // 전체 free block의 값을 저장 

        if((csize - asize) >= MINIMUM) { // 차가 적어도 24바이트일 경우, header와 footer를 바꾼다
                PUT(HDRP(bp), PACK(asize,1));
                PUT(FTRP(bp), PACK(asize,1));
                removeBlock(bp);
                bp = NEXT_BLKP(bp);
                PUT(HDRP(bp), PACK(csize-asize, 0));
                PUT(FTRP(bp), PACK(csize-asize, 0));
                coalesce(bp);
        }

        // 남아있는 공간이 충분하지 않을경우 split 하지 않는다
        else {
                PUT(HDRP(bp), PACK(csize, 1));
                PUT(FTRP(bp), PACK(csize, 1));
                removeBlock(bp);
        }
}

static void *find_fit(size_t asize){
        void *bp;

        //block의 처음 부터 끝까지 탐색하면서 asize만큼 할당할 수 있는 free block이 존재 하는지 탐색한다.
        for( bp = free_listp; GET_ALLOC(HDRP(bp)) == 0; bp = NEXT_FREEP(bp)){
                if(asize <= (size_t)GET_SIZE(HDRP(bp)))
                        return bp;
        }
        return NULL; // no fit

}

void *coalesce(void *bp) {

        size_t prev_alloc;
        prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))) || PREV_BLKP(bp) == bp;
        // 이전 블럭의 할당 여부 YES = 1, NO = 0
        size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
        // 다음 블럭의 할당 여부 YES = 1, NO = 0
        size_t size = GET_SIZE(HDRP(bp));
        // 현재 블럭의 크기를 받아옵니다
        
	// 이전 블럭의 최하위 비트가 1이고 (할당), 다음 블럭의 최하위 bit가 0인 경우 (비할당)
	if(prev_alloc && !next_alloc){
        	size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
		removeBlock(NEXT_BLKP(bp));
		PUT(HDRP(bp), PACK(size, 0));
		PUT(FTRP(bp), PACK(size, 0));	
	}

        // case 2 : 이전 블럭의 최하위 비트가 0이고 (비할당), 다음 블럭의 최하위 bit가 1인 경우 (할당) 
        else if(!prev_alloc && next_alloc){
                size += GET_SIZE(HDRP(PREV_BLKP(bp)));
                bp = PREV_BLKP(bp);
                removeBlock(bp);
                PUT(HDRP(bp), PACK(size,0));
                PUT(FTRP(bp), PACK(size,0));
        }

        // case 3 : 이전 블럭의 최하위 비트가 0이고 (비할당), 다음 블럭의 최하위 bit가 0인 경우(비할당)
        else if(!prev_alloc && !next_alloc){
        	size += GET_SIZE(HDRP(PREV_BLKP(bp))) + 
				GET_SIZE(HDRP(NEXT_BLKP(bp)));
		removeBlock(PREV_BLKP(bp));
		removeBlock(NEXT_BLKP(bp));
		bp = PREV_BLKP(bp);
		PUT(HDRP(bp), PACK(size, 0));
		PUT(FTRP(bp), PACK(size, 0));	
	}

        insertAtFront(bp);

        return bp;

}

static void insertAtFront(void *bp){
        NEXT_FREEP(bp) = free_listp; // bp의 next를 free block의 처음을 가리키게함
        PREV_FREEP(free_listp) = bp; // free block의 처음의 이전 block을 bp가 가리키게함
        PREV_FREEP(bp) = NULL; // bp의 이전 free block은 null을 가리키게함
        free_listp = bp; // 처음 free block이 bp를 가리키게 함
}

static void removeBlock(void *bp){
        if(PREV_FREEP(bp)) 	// previous block이 존재 할 경우
		NEXT_FREEP(PREV_FREEP(bp)) = NEXT_FREEP(bp);
        else 			// previous block이 존재하지 않을 경우
                free_listp = NEXT_FREEP(bp);
	PREV_FREEP(NEXT_FREEP(bp)) = PREV_FREEP(bp);
}
