package test

import "math/rand"

type sliceCache struct {
	Data []byte
	Size int64
}

func newSliceCache(size int64) *sliceCache {
	if size <= 0 {
		size = 1024 * 1024 // 1M
	}
	if size < 16 {
		return nil
	}
	c := new(sliceCache)
	c.Data = make([]byte, size)
	c.Size = size
	return c
}

func genSliceFileBytes(slice *sliceCache) {
	if slice.Size <= 0 {
		return
	}
	rand.Read(slice.Data)
}
