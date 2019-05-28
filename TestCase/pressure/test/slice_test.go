package test

import (
	"fmt"
	"testing"

	"github.com/stretchr/testify/assert"
)

func Test_genSliceFileBytes(t *testing.T) {

	t.Run("copy", func(t *testing.T) {
		slice := newSliceCache(1024)
		genSliceFileBytes(slice)
		expected := fmt.Sprintf("%x", slice.Data)

		head := "hello"
		copy(slice.Data, []byte(head))
		actual := fmt.Sprintf("%x", slice.Data)

		assert.Equal(t, []byte(expected)[len(head)*2:], []byte(actual)[len(head)*2:])
	})

	t.Run("diff", func(t *testing.T) {
		slice := newSliceCache(1024)
		genSliceFileBytes(slice)

		slice2 := newSliceCache(1024)
		genSliceFileBytes(slice2)

		assert.NotEqual(t, slice, slice2)
	})
}
