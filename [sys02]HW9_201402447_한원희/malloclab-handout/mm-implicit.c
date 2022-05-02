/*
 * mm-implicit.c - an empty malloc package
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
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))
#define SIZE_PTR(p) ((size_t*)(((char*)(p)) - SIZE_T_SIZE))


/* $begin mallocmacros */

#define WSIZE 4			//word 크기
#define DSIZE 8			//double word 크기
#define CHUNKSIZE (1<<12)	//초기 heap크기 결정(bytes)(4096)
#define OVERHEAD 8		//header+footer 크기(실제 데이터 저장되는공간 아님)

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define PACK(size, alloc) ((size) | (alloc))	//PACK 매크로 사용하여 size와 alloc값을 하나의 word로 묶음(쉽게 header와 footer에 저장가능)

#define GET(p) (*(unsigned int*)(p))		//포인터 p가 가리키는 위치에서 word크기값 읽음
#define PUT(p, val) (*(unsigned int*)(p) = (val))	//포인터 p가 가리키는 곳에 word크기의 val값 씀

#define GET_SIZE(p) (GET(p) & ~0x7)		//포인터 p가 가리키는 곳에서 한word읽은 후 하위 3비트 버림(header에서 블록사이즈 읽는것과 같음)
#define GET_ALLOC(p) (GET(p) & 0x1)		//포인터 p가 가리키는 곳에서 한word를 읽은 후 하위 1비트 읽음(블록할당여부)

#define HDRP(bp) ((char*)(bp) - WSIZE)		//주어진 포인터 bp의 header의 주소 계산
#define FTRP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)	//주어진 포인터 bp의footer의 주소 계산
#define NEXT_BLKP(bp) ((char*)(bp) + GET_SIZE((char*)(bp) - WSIZE))//주어진 포인터 bp를 이용하여 다음 블록의 주소 계산
#define PREV_BLKP(bp) ((char*)(bp) - GET_SIZE((char*)(bp) - DSIZE))//주어진 포인터 bp를 이용하여 이전 블록의 주소 계산

/* $end mallocmacros */


/* function prototypes for internal helper routines */
static void *coalesce(void *bp);
static void place(void *bp, size_t asize);
static void *extend_heap(size_t words);
static void *find_fit(size_t asize);
static void *next_fit(size_t asize);

/* Global variables */
static char *heap_listp = 0;
static char *next_listp = 0;

/*
 * Initialize: return -1 on error, 0 on success.
 */
int mm_init(void) 
{
	// 초기 empty heap 생성
	if ((heap_listp = mem_sbrk(4*WSIZE)) == NULL)
	//heap의 크기를 incr, 여기서는 4*WSIZE(4)=16 만큼 공간 할당
		return -1;//실패시 -1 반환
	
	PUT(heap_listp, 0);	
	//정렬을 위해 의미없는 값 삽입
	PUT(heap_listp+WSIZE, PACK(OVERHEAD, 1));
	//heap_listp+WSIZE(4)의 위치에
	//size OVERHEAD(8)와 alloc값 1을 하나의 word로 묶어서 씀
	PUT(heap_listp+DSIZE, PACK(OVERHEAD, 1));
	//heap_listp+DSIZE(8)의 위치에
	//size OVERHEAD(8)와 alloc값 1을 하나의 word로 묶어서 씀
	PUT(heap_listp+WSIZE+DSIZE, PACK(0, 1));
	//heap_listp+WSIZE(4)+DISZE(8)의 위치에
	//size 0과 allocr값 1을 하나의 word로 묶어서 씀
	//epilogue handler
	heap_listp += DSIZE;
	//heap_listp 를 DSIZE(8)만큼 이동
	//이 위치가 데이터가 들어가기 시작하는 위치이다.
	if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
	//CHUNKSIZE(4096)을 WSIZE(4)로 나눈 만큼 확장
		return -1;//실패시 -1 반환
	return 0;
}
/*
 * malloc
 */
void *malloc (size_t size) {
    size_t asize;	//조정된 블록크기
    size_t extendsize;	//Amount to extend heap if no fit
    char *bp;

    //Ignore spurious requests
    // 입력받은 사이즈가 0이라면 무시
    if(size == 0)
        return NULL;

    //overhead와 alignment reqs. 포함한 조정된 블록크기
    if(size <= DSIZE)
        asize = 2*DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

    //fit한 free list 탐색 find_fit, next_fit 을 정합니다.
    if((bp = next_fit(asize)) != NULL){
        place(bp, asize);
        return bp;
    }

    //fit 발견안됨. get more memory and place the block
    extendsize = MAX(asize, CHUNKSIZE);
    if((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);	//확장한 위치 bp에 asize 크기를 할당
    return bp;
}

/*
 * free
 */
void free (void *bp) {
   	if(!bp) return; 	//잘못된 free요청인 경우 종료. 이전 프로시저로 return

	size_t size = GET_SIZE(HDRP(bp));  //bp의 헤더에서 블록크기 읽어옴
   	//header와 footer의 최하위 1비트(1, 할당된 상태)만 수정

	PUT(HDRP(bp), PACK(size, 0));	//bp의 header에 블록크기와 alloc = 0을 저장
	PUT(FTRP(bp), PACK(size, 0));	//fotter에 저장
	
	next_listp = coalesce(bp);	//주변에 빈 블록 있을 시 병합

}

/*
 * realloc - you may want to look at mm-naive.c
 */
void *realloc(void *oldptr, size_t size) {
    
    size_t oldsize;	// 기존 블록의 사이즈
    void *newptr;	// 새로 할당받은 블록

    //if size == 0 then this is just free, and we return NULL.
    if(size == 0){
        free(oldptr);
        return 0;
    }

    //if oldptr is NULL, then this is just malloc.
    if(oldptr == NULL){
        return malloc(size);
    }

    newptr = malloc(size);

    //if realloc() fails the original block is left untouched
    if(!newptr){
        return 0;
    }

    //copy the old data
    oldsize = *SIZE_PTR(oldptr);
    if(size < oldsize) oldsize = size;
    memcpy(newptr, oldptr, oldsize);

    //free the old block
    free(oldptr);

    return newptr;
}

/*
 * calloc - you may want to look at mm-naive.c
 * This function is not tested by mdriver, but it is
 * needed to run the traces.
 */
void *calloc (size_t nmemb, size_t size) {

  size_t bytes = nmemb * size;
  void *newptr;

  newptr = malloc(bytes);
  memset(newptr, 0, bytes);

  return newptr;
}


/*
 * Return whether the pointer is in the heap.
 * May be useful for debugging.
 */
static int in_heap(const void *p) {
    return p < mem_heap_hi() && p >= mem_heap_lo();
}

/*
 * Return whether the pointer is aligned.
 * May be useful for debugging.
 */
static int aligned(const void *p) {
    return (size_t)ALIGN(p) == (size_t)p;
}

/*
 * mm_checkheap
 */
void mm_checkheap(int verbose) {
}


static void *coalesce(void *bp)
{
	size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
	//현재 블록의 이전 블록의 할당 여부 확인
	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
	//현재 블록의 다음 블록의 할당 여부 확인
	size_t size = GET_SIZE(HDRP(bp));
	//현재 블록의 사이즈를 얻어냄
		/*case 1 : 이전 블록 다읍블록 모두 최하위 bit가 1인 경우
			둘 다 할당되어 있다는 뜻임으로 병합없이 return */
	if(prev_alloc && next_alloc){
		return bp;
	}
	else if(prev_alloc && !next_alloc){
		/*case 2 : 이전 블록의 최하위 bit 1이고 (allocated)
			다음 블록의 최하위 bit 0이면(free)
			다음 블록과 병합한뒤 return */
		size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
		//현재 블록의 size에 다음 블록의 size를 더한 값 만큼
		//한 블록으로 합칠 수 있다.
		PUT(HDRP(bp), PACK(size, 0));
		//현재 블록 포인터의 header에 증가한size와 할당값 0을 적음
		PUT(FTRP(bp), PACK(size, 0));
		//현재 블록 포인터의 footer에 증가한size와 할당갑 0을 적음
	}
	else if(!prev_alloc && next_alloc){
		/*case3 : 이전 블록의 최하위 bit 0이고(free)
			다음 블록의 최하위 bit 1이면(allocated)
			이전 블록과 병합한뒤 return */
		size += GET_SIZE(HDRP(PREV_BLKP(bp)));
		//현재 블록의 size에 이전 블록의 size를 더한 값 만큼
		//한 블록으로 합칠 수 있다.
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		//현재 블록 포인터의 header에 증가한size와 할당값 0을 적음
		PUT(FTRP(bp), PACK(size, 0));
		//현재 블록 포인터의 footer에 증가한size와 할당갑 0을 적음
		//여기서 마치면 이전 블럭의 header가 블록의 포인터가 되는데
		//합쳐진 블록의 header를 블록 포인터로 만들어 주기 위해
		bp = PREV_BLKP(bp);//현재 블록의 header를 구해 bp에 넣어줌
	}
	else{
		/* case4 : 이전 블록의 최하위 bit 0이고(free)
			다음 블록의 최하위 bit 0이면(free)
			이전, 다음 블록과 병합한뒤 return */
		size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
		//size에 이전 블럭과 다음 블럭의 크기를 추가
		//현재 블록의 size에 이전 블록의 size와 다음 블록의 size를 
		//더한 값 만큼 한 블록으로 합칠 수 있다.
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		//현재 블록 포인터의 header에 증가한size와 할당값 0을 적음
		PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
		//현재 블록 포인터의 footer에 증가한size와 할당갑 0을 적음
		bp = PREV_BLKP(bp);
		//위와 같은 이유로 블록포인터를 header로 옮겨줌
	}
	return bp;
}

static void *extend_heap(size_t words)
{
	char *bp;
	size_t size;

	size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
	//인자로 받은 words를 2로 나눈 값이 
	//0(거짓)인경우 size = words*WSIZE
	//1(참)인경우 size = (words+1)*WSIZE
	if((long)(bp = mem_sbrk(size)) == -1)//mem_brk로 size공간만큼 할당
		return NULL;//실패서 NULL반환
	
	PUT(HDRP(bp), PACK(size, 0));
	//할당받은 블록포인터 bp의 header에 size와 값 0을 적어줌
	PUT(FTRP(bp), PACK(size, 0));
	//할당받은 블록포인터 bp의 footer에 size와 값 0을 적어줌
	PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));
	//할당받은 블록포인터 bp의 다음블록의 header를 size 0, 값 1로 초기화
	next_listp = coalesce(bp);
	return next_listp;
	//할당받은 블록을 coalesce를 이용하여 병합할수 있다면 한 후 bp를반환
	//next_fit을 위해 next_listp에 줌
}	

static void place(void *bp , size_t asize){
    size_t csize = GET_SIZE(HDRP(bp));
	//입력받은 블록의 사이즈

    if((csize - asize) >= (2*DSIZE)){
	//입력받은 블록의 사이즈와 입력받은 사이즈의 차가 16보다 크거나 같으면
	//블록을 나누어 준다음 할당을 해준다.

        PUT(HDRP(bp), PACK(asize, 1));
	//입력받은 블록의 헤더에 입력받은 사이즈 만큼 할당됨을 저장한다.
        PUT(FTRP(bp), PACK(asize, 1));
	//입력받은 블록의 풋터에 입력받은 사이즈 만큼 할당됨을 저장한다.

        bp = NEXT_BLKP(bp);
	//블록의 헤더와 풋터를 설정해 주었으므로 나머지 블록이 다음 블록이된다.

        PUT(HDRP(bp), PACK(csize-asize, 0));
	//입력받은 블록에서 할당 받고 남은 블록의 헤더에 남은 사이즈를 저장한다.
        PUT(FTRP(bp), PACK(csize-asize, 0));
	//입력받은 블록에서 할당 받고 남은 블록의 풋터에 남은 사이즈를 저장한다.

	next_listp = bp;
    }
    else{
	//사이즈의 차가 16보다 작으면 블록을 나누지 않고 입력받은 블록을 모두 할당해줍니다.
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}


static void *find_fit(size_t asize){
    void *bp;

    for(bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){	
	// 마지막으로 사용했던 블록부터 블록 사이즈가 0이 될때까지의 블록을 본다
        if(!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))){
	// 해당 블록이 free블록이고 요청 사이즈보다 크거나 같으면 
            return bp;
        }
    }
    return NULL;//No fit	
}

static void *next_fit(size_t asize){
	void *bp;

	for(bp = next_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){
		if(!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))){
			return bp;
		}
	}
	return NULL;
}
