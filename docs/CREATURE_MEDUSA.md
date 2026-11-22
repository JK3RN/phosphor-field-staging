# PF-MEDUSA — design, mathematics and provenance

PF-MEDUSA (`pf-medusa`, implemented in `src/creature.h`) is the built-in creature of Phosphor
Field. It was designed and implemented for this project in November 2026.

## Approach

PF-MEDUSA is an explicit 3D body that is rebuilt on every frame, tumbled in space and projected
with mild perspective. It is a pure function of the point count `N` and the phase `t`. It keeps
no state, allocates nothing and gives bit-identical results for the same input.

### Anatomy and point budget

| Part | Share of N | Construction |
| --- | --- | --- |
| Bell | 24 % | Surface of revolution over polar fraction `a` and azimuth `b`, sampled on a golden-angle (Vogel) spiral balanced by area. Under 3D rotation the spiral produces drifting moiré. 16 marginal lobes and a slowly rotating five-fold asymmetry. |
| Margin and velum | 8 % | A bright rim ring and an inward shelf |
| Oral arms | 18 % | Four twisting, ruffled ribbons. The ruffle wave travels along the arms, and their length follows the beat with a delay. |
| Tentacles | 50 % | 28 tentacles of different lengths. A point at position `s` follows the bell rim **as it was 2.4·s phase units earlier**, so each stroke travels down as a wave. Two superposed 3D undulations and a faint current are added. |

### Key equations

- **Beat.** `beat(t) = sin²(π·√frac(t/2π))`. The square root makes the contraction fast and the release slow, and the function is continuous across cycles.
- **Light packet.** A sample's position along a body part is the exact quantile of the mixture density `(1−g)·uniform + g·gauss(front, width)` on `[0,1]`, solved with bracketed Newton iterations.
  - This is strictly monotone and continuous in time, and the packet fades out naturally at the ends.
  - The front runs from −0.3 to 2.3 per beat: apex to rim on [0,1], rim to tips on [1,2]. It therefore starts and ends more than 4σ outside the body, and the cycle wrap is invisible.
  - The bell uses the area fraction, so its ring of light widens as it nears the rim.
- **Pose.**
  - Yaw is `0.19·t`.
  - Pitch is `0.34 + 0.10·sin(0.13t) + 0.62·sin⁵(0.041t)`. The sin⁵ term produces a rare deep tilt about once a minute.
  - Roll is `0.14·sin(0.29t + 1)`.
  - The projection is `f = 560/(560 − z)`.

## Development log

1. **First model.** The bell worked at once. The tentacles read as a stiff curtain, and the light packet was invisible.
2. **Tentacles.** They got 3D undulations and individual lengths, and the light became one continuous front.
3. **Light diagnosis.** Front and back faces had saturated the bell, and the first warp only gathered neighbouring samples into one pixel. It was replaced by inverse-CDF sampling of a mixture, and the bell became more translucent.
4. **Tilt and speed.** The rare deep tilt was added. Per-frame constants were hoisted, and the Newton solve became warm-started.
5. **Continuity fix.** The continuity test found the light packet popping at the cycle wrap. The front span was moved beyond ±4σ.

## Measured properties

These were measured with GCC 16.2.1 at `-O2` on x86_64.

- **Generation time.** 2.4 ms per frame at N = 12 000 and 9.3 ms at N = 50 000. This is the creature maths alone, without rendering.
- **Stress run.** 411 million samples were checked over N from 1 to 50 000 and phases up to 10⁷. None was non-finite and none fell outside ±200. The range was x −142.5…144.9 and y −120.9…188.0.
- **Continuity.** The maximum sample displacement per 4·10⁻⁴ phase step is 0.105 units, including across the beat wrap. This is asserted in `tests/test_generator.c`.

## Provenance and rights

- All code, constants and structure were written for this project.
- During development the maintainer's private prototype contained point figures of unclear authorship. They were analysed only in order to choose a deliberately different method: a 3D body instead of a 2D formula warp. None of their code, constants, structure or designs was used, and none of it is part of this repository or its history.
- The mathematics is general knowledge: surfaces of revolution, Vogel phyllotaxis, rotation matrices, central projection, the normal CDF and Newton's method.
- A jellyfish is a natural subject. No reference images, videos or third-party jellyfish sketches were used.
- Other generative works also show jellyfish-like creatures. Sharing a subject is not copying expression. This is a documented assessment, not legal advice.
