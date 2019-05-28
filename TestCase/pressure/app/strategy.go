package app

import (
	"fmt"
	"math/rand"
	"strings"
)

// Strategy for const
type Strategy int32

// Strategy for test
const (
	S50K Strategy = iota
	S100K
	S200K
	S300K
	S400K
	S500K
	S600K
	S700K
	S800K
	S900K
	S1M
	S5M
	S10M
	S100M
	S600M
	S1G
	S2G
	SMax // Do not use this.
)

const (
	c1K = 1024
	c1M = 1024 * 1024
)

var mapSample map[Strategy][]int32
var samples []string

func init() {
	mapSample = make(map[Strategy][]int32)
	mapSample[S50K] = []int32{c1K, c1K * 5, c1K * 10, c1K * 20, c1K * 30, c1K * 40}
	mapSample[S100K] = []int32{c1K * 50, c1K * 60, c1K * 70, c1K * 80, c1K * 90, c1K * 100}
	mapSample[S200K] = []int32{c1K * 110, c1K * 120, c1K * 140, c1K * 160, c1K * 180, c1K * 200}
	mapSample[S300K] = []int32{c1K * 210, c1K * 220, c1K * 240, c1K * 260, c1K * 280, c1K * 300}
	mapSample[S400K] = []int32{c1K * 310, c1K * 320, c1K * 340, c1K * 360, c1K * 380, c1K * 400}
	mapSample[S500K] = []int32{c1K * 410, c1K * 420, c1K * 440, c1K * 460, c1K * 480, c1K * 500}

	mapSample[S600K] = []int32{c1K * 510, c1K * 520, c1K * 540, c1K * 560, c1K * 580, c1K * 600}
	mapSample[S700K] = []int32{c1K * 610, c1K * 620, c1K * 640, c1K * 660, c1K * 680, c1K * 700}
	mapSample[S800K] = []int32{c1K * 710, c1K * 720, c1K * 740, c1K * 760, c1K * 780, c1K * 800}
	mapSample[S900K] = []int32{c1K * 810, c1K * 820, c1K * 840, c1K * 860, c1K * 880, c1K * 900}
	mapSample[S1M] = []int32{c1K * 910, c1K * 920, c1K * 940, c1K * 960, c1K * 980, c1M}

	mapSample[S5M] = []int32{c1M, c1M * 2, c1M * 3, c1M * 4, c1M * 5}
	mapSample[S10M] = []int32{c1M * 5, c1M * 6, c1M * 7, c1M * 8, c1M * 9, c1M * 10}
	mapSample[S100M] = []int32{c1M * 10, c1M * 20, c1M * 40, c1M * 50, c1M * 70, c1M * 100}
	mapSample[S600M] = []int32{c1M * 100, c1M * 200, c1M * 300, c1M * 400, c1M * 500, c1M * 600}
	mapSample[S1G] = []int32{c1M * 600, c1M * 800, c1M * 900, c1M * 1024, c1M * 1300, c1M * 1500}
	mapSample[S2G] = []int32{c1M * 1024, c1M * 1300, c1M * 1500, c1M * 1700, c1M * 1800, c1M * 2000}

	samples = []string{"S50K", "S100K", "S200K", "S300K", "S400K", "S500K", "S600K",
		"S700K", "S800K", "S900K", "S1M", "S5M", "S10M", "S100M", "S600M", "S1G", "S2G"}
}

func (s Strategy) String() string {
	return samples[s]
}

// StrategyFromString parse Strategy from string.
func StrategyFromString(s string) (Strategy, error) {
	s = strings.ToUpper(s)
	for i, v := range samples {
		if v == s {
			return Strategy(i), nil
		}
	}

	return SMax, fmt.Errorf("Invalid sample[%s]. You must choose %v", s, samples)
}

// Samples returns the samples by strategy.
func Samples(s Strategy) []int32 {
	return mapSample[s]
}

// RandomStrategy get random strategy.
func RandomStrategy() Strategy {
	s := rand.Intn(int(SMax))
	return Strategy(s)
}
