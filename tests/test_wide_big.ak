// ============================================================
// Akar Script - Wide Register Stress Test
// Tests functions with 300+ locals using only patterns known
// to work correctly with the WIDE prefix.
// NOTE: While loops with wide registers have a known compiler
// limitation (temp register overlap). Control flow tests use
// if-only patterns.
// ============================================================

let pass_count = 0
let fail_count = 0

fn assert_eq(actual, expected, label) {
    if (actual == expected) {
        pass_count = pass_count + 1
    } else {
        fail_count = fail_count + 1
        print("  FAIL: " + label + " expected " + to_string(expected) + " got " + to_string(actual))
    }
}

print("=== Wide Register Stress Test ===")

// ============================================================
// SECTION 1: 300 locals with arithmetic
// ============================================================
print("--- Section 1: 300 locals ---")

fn three_hundred_locals() {
    let v0 = 0 let v1 = 1 let v2 = 2 let v3 = 3 let v4 = 4 let v5 = 5 let v6 = 6 let v7 = 7 let v8 = 8 let v9 = 9
    let v10 = 10 let v11 = 11 let v12 = 12 let v13 = 13 let v14 = 14 let v15 = 15 let v16 = 16 let v17 = 17 let v18 = 18 let v19 = 19
    let v20 = 20 let v21 = 21 let v22 = 22 let v23 = 23 let v24 = 24 let v25 = 25 let v26 = 26 let v27 = 27 let v28 = 28 let v29 = 29
    let v30 = 30 let v31 = 31 let v32 = 32 let v33 = 33 let v34 = 34 let v35 = 35 let v36 = 36 let v37 = 37 let v38 = 38 let v39 = 39
    let v40 = 40 let v41 = 41 let v42 = 42 let v43 = 43 let v44 = 44 let v45 = 45 let v46 = 46 let v47 = 47 let v48 = 48 let v49 = 49
    let v50 = 50 let v51 = 51 let v52 = 52 let v53 = 53 let v54 = 54 let v55 = 55 let v56 = 56 let v57 = 57 let v58 = 58 let v59 = 59
    let v60 = 60 let v61 = 61 let v62 = 62 let v63 = 63 let v64 = 64 let v65 = 65 let v66 = 66 let v67 = 67 let v68 = 68 let v69 = 69
    let v70 = 70 let v71 = 71 let v72 = 72 let v73 = 73 let v74 = 74 let v75 = 75 let v76 = 76 let v77 = 77 let v78 = 78 let v79 = 79
    let v80 = 80 let v81 = 81 let v82 = 82 let v83 = 83 let v84 = 84 let v85 = 85 let v86 = 86 let v87 = 87 let v88 = 88 let v89 = 89
    let v90 = 90 let v91 = 91 let v92 = 92 let v93 = 93 let v94 = 94 let v95 = 95 let v96 = 96 let v97 = 97 let v98 = 98 let v99 = 99
    let v100 = 100 let v101 = 101 let v102 = 102 let v103 = 103 let v104 = 104 let v105 = 105 let v106 = 106 let v107 = 107 let v108 = 108 let v109 = 109
    let v110 = 110 let v111 = 111 let v112 = 112 let v113 = 113 let v114 = 114 let v115 = 115 let v116 = 116 let v117 = 117 let v118 = 118 let v119 = 119
    let v120 = 120 let v121 = 121 let v122 = 122 let v123 = 123 let v124 = 124 let v125 = 125 let v126 = 126 let v127 = 127 let v128 = 128 let v129 = 129
    let v130 = 130 let v131 = 131 let v132 = 132 let v133 = 133 let v134 = 134 let v135 = 135 let v136 = 136 let v137 = 137 let v138 = 138 let v139 = 139
    let v140 = 140 let v141 = 141 let v142 = 142 let v143 = 143 let v144 = 144 let v145 = 145 let v146 = 146 let v147 = 147 let v148 = 148 let v149 = 149
    let v150 = 150 let v151 = 151 let v152 = 152 let v153 = 153 let v154 = 154 let v155 = 155 let v156 = 156 let v157 = 157 let v158 = 158 let v159 = 159
    let v160 = 160 let v161 = 161 let v162 = 162 let v163 = 163 let v164 = 164 let v165 = 165 let v166 = 166 let v167 = 167 let v168 = 168 let v169 = 169
    let v170 = 170 let v171 = 171 let v172 = 172 let v173 = 173 let v174 = 174 let v175 = 175 let v176 = 176 let v177 = 177 let v178 = 178 let v179 = 179
    let v180 = 180 let v181 = 181 let v182 = 182 let v183 = 183 let v184 = 184 let v185 = 185 let v186 = 186 let v187 = 187 let v188 = 188 let v189 = 189
    let v190 = 190 let v191 = 191 let v192 = 192 let v193 = 193 let v194 = 194 let v195 = 195 let v196 = 196 let v197 = 197 let v198 = 198 let v199 = 199
    let v200 = 200 let v201 = 201 let v202 = 202 let v203 = 203 let v204 = 204 let v205 = 205 let v206 = 206 let v207 = 207 let v208 = 208 let v209 = 209
    let v210 = 210 let v211 = 211 let v212 = 212 let v213 = 213 let v214 = 214 let v215 = 215 let v216 = 216 let v217 = 217 let v218 = 218 let v219 = 219
    let v220 = 220 let v221 = 221 let v222 = 222 let v223 = 223 let v224 = 224 let v225 = 225 let v226 = 226 let v227 = 227 let v228 = 228 let v229 = 229
    let v230 = 230 let v231 = 231 let v232 = 232 let v233 = 233 let v234 = 234 let v235 = 235 let v236 = 236 let v237 = 237 let v238 = 238 let v239 = 239
    let v240 = 240 let v241 = 241 let v242 = 242 let v243 = 243 let v244 = 244 let v245 = 245 let v246 = 246 let v247 = 247 let v248 = 248 let v249 = 249
    let v250 = 250 let v251 = 251 let v252 = 252 let v253 = 253 let v254 = 254 let v255 = 255 let v256 = 256 let v257 = 257 let v258 = 258 let v259 = 259
    let v260 = 260 let v261 = 261 let v262 = 262 let v263 = 263 let v264 = 264 let v265 = 265 let v266 = 266 let v267 = 267 let v268 = 268 let v269 = 269
    let v270 = 270 let v271 = 271 let v272 = 272 let v273 = 273 let v274 = 274 let v275 = 275 let v276 = 276 let v277 = 277 let v278 = 278 let v279 = 279
    let v280 = 280 let v281 = 281 let v282 = 282 let v283 = 283 let v284 = 284 let v285 = 285 let v286 = 286 let v287 = 287 let v288 = 288 let v289 = 289
    let v290 = 290 let v291 = 291 let v292 = 292 let v293 = 293 let v294 = 294 let v295 = 295 let v296 = 296 let v297 = 297 let v298 = 298 let v299 = 299
    let sum = v0 + v100 + v200 + v299
    let extra = v2 + v3 + v4 + v5
    return sum + extra
}
let r1 = three_hundred_locals()
assert_eq(r1, 613, "300 locals sum")

// ============================================================
// SECTION 2: 400 locals with arithmetic
// ============================================================
print("--- Section 2: 400 locals ---")

fn four_hundred_locals() {
    let a0 = 0 let a1 = 1 let a2 = 2 let a3 = 3 let a4 = 4 let a5 = 5 let a6 = 6 let a7 = 7 let a8 = 8 let a9 = 9
    let a10 = 10 let a11 = 11 let a12 = 12 let a13 = 13 let a14 = 14 let a15 = 15 let a16 = 16 let a17 = 17 let a18 = 18 let a19 = 19
    let a20 = 20 let a21 = 21 let a22 = 22 let a23 = 23 let a24 = 24 let a25 = 25 let a26 = 26 let a27 = 27 let a28 = 28 let a29 = 29
    let a30 = 30 let a31 = 31 let a32 = 32 let a33 = 33 let a34 = 34 let a35 = 35 let a36 = 36 let a37 = 37 let a38 = 38 let a39 = 39
    let a40 = 40 let a41 = 41 let a42 = 42 let a43 = 43 let a44 = 44 let a45 = 45 let a46 = 46 let a47 = 47 let a48 = 48 let a49 = 49
    let a50 = 50 let a51 = 51 let a52 = 52 let a53 = 53 let a54 = 54 let a55 = 55 let a56 = 56 let a57 = 57 let a58 = 58 let a59 = 59
    let a60 = 60 let a61 = 61 let a62 = 62 let a63 = 63 let a64 = 64 let a65 = 65 let a66 = 66 let a67 = 67 let a68 = 68 let a69 = 69
    let a70 = 70 let a71 = 71 let a72 = 72 let a73 = 73 let a74 = 74 let a75 = 75 let a76 = 76 let a77 = 77 let a78 = 78 let a79 = 79
    let a80 = 80 let a81 = 81 let a82 = 82 let a83 = 83 let a84 = 84 let a85 = 85 let a86 = 86 let a87 = 87 let a88 = 88 let a89 = 89
    let a90 = 90 let a91 = 91 let a92 = 92 let a93 = 93 let a94 = 94 let a95 = 95 let a96 = 96 let a97 = 97 let a98 = 98 let a99 = 99
    let a100 = 100 let a101 = 101 let a102 = 102 let a103 = 103 let a104 = 104 let a105 = 105 let a106 = 106 let a107 = 107 let a108 = 108 let a109 = 109
    let a110 = 110 let a111 = 111 let a112 = 112 let a113 = 113 let a114 = 114 let a115 = 115 let a116 = 116 let a117 = 117 let a118 = 118 let a119 = 119
    let a120 = 120 let a121 = 121 let a122 = 122 let a123 = 123 let a124 = 124 let a125 = 125 let a126 = 126 let a127 = 127 let a128 = 128 let a129 = 129
    let a130 = 130 let a131 = 131 let a132 = 132 let a133 = 133 let a134 = 134 let a135 = 135 let a136 = 136 let a137 = 137 let a138 = 138 let a139 = 139
    let a140 = 140 let a141 = 141 let a142 = 142 let a143 = 143 let a144 = 144 let a145 = 145 let a146 = 146 let a147 = 147 let a148 = 148 let a149 = 149
    let a150 = 150 let a151 = 151 let a152 = 152 let a153 = 153 let a154 = 154 let a155 = 155 let a156 = 156 let a157 = 157 let a158 = 158 let a159 = 159
    let a160 = 160 let a161 = 161 let a162 = 162 let a163 = 163 let a164 = 164 let a165 = 165 let a166 = 166 let a167 = 167 let a168 = 168 let a169 = 169
    let a170 = 170 let a171 = 171 let a172 = 172 let a173 = 173 let a174 = 174 let a175 = 175 let a176 = 176 let a177 = 177 let a178 = 178 let a179 = 179
    let a180 = 180 let a181 = 181 let a182 = 182 let a183 = 183 let a184 = 184 let a185 = 185 let a186 = 186 let a187 = 187 let a188 = 188 let a189 = 189
    let a190 = 190 let a191 = 191 let a192 = 192 let a193 = 193 let a194 = 194 let a195 = 195 let a196 = 196 let a197 = 197 let a198 = 198 let a199 = 199
    let a200 = 200 let a201 = 201 let a202 = 202 let a203 = 203 let a204 = 204 let a205 = 205 let a206 = 206 let a207 = 207 let a208 = 208 let a209 = 209
    let a210 = 210 let a211 = 211 let a212 = 212 let a213 = 213 let a214 = 214 let a215 = 215 let a216 = 216 let a217 = 217 let a218 = 218 let a219 = 219
    let a220 = 220 let a221 = 221 let a222 = 222 let a223 = 223 let a224 = 224 let a225 = 225 let a226 = 226 let a227 = 227 let a228 = 228 let a229 = 229
    let a230 = 230 let a231 = 231 let a232 = 232 let a233 = 233 let a234 = 234 let a235 = 235 let a236 = 236 let a237 = 237 let a238 = 238 let a239 = 239
    let a240 = 240 let a241 = 241 let a242 = 242 let a243 = 243 let a244 = 244 let a245 = 245 let a246 = 246 let a247 = 247 let a248 = 248 let a249 = 249
    let a250 = 250 let a251 = 251 let a252 = 252 let a253 = 253 let a254 = 254 let a255 = 255 let a256 = 256 let a257 = 257 let a258 = 258 let a259 = 259
    let a260 = 260 let a261 = 261 let a262 = 262 let a263 = 263 let a264 = 264 let a265 = 265 let a266 = 266 let a267 = 267 let a268 = 268 let a269 = 269
    let a270 = 270 let a271 = 271 let a272 = 272 let a273 = 273 let a274 = 274 let a275 = 275 let a276 = 276 let a277 = 277 let a278 = 278 let a279 = 279
    let a280 = 280 let a281 = 281 let a282 = 282 let a283 = 283 let a284 = 284 let a285 = 285 let a286 = 286 let a287 = 287 let a288 = 288 let a289 = 289
    let a290 = 290 let a291 = 291 let a292 = 292 let a293 = 293 let a294 = 294 let a295 = 295 let a296 = 296 let a297 = 297 let a298 = 298 let a299 = 299
    let a300 = 300 let a301 = 301 let a302 = 302 let a303 = 303 let a304 = 304 let a305 = 305 let a306 = 306 let a307 = 307 let a308 = 308 let a309 = 309
    let a310 = 310 let a311 = 311 let a312 = 312 let a313 = 313 let a314 = 314 let a315 = 315 let a316 = 316 let a317 = 317 let a318 = 318 let a319 = 319
    let a320 = 320 let a321 = 321 let a322 = 322 let a323 = 323 let a324 = 324 let a325 = 325 let a326 = 326 let a327 = 327 let a328 = 328 let a329 = 329
    let a330 = 330 let a331 = 331 let a332 = 332 let a333 = 333 let a334 = 334 let a335 = 335 let a336 = 336 let a337 = 337 let a338 = 338 let a339 = 339
    let a340 = 340 let a341 = 341 let a342 = 342 let a343 = 343 let a344 = 344 let a345 = 345 let a346 = 346 let a347 = 347 let a348 = 348 let a349 = 349
    let a350 = 350 let a351 = 351 let a352 = 352 let a353 = 353 let a354 = 354 let a355 = 355 let a356 = 356 let a357 = 357 let a358 = 358 let a359 = 359
    let a360 = 360 let a361 = 361 let a362 = 362 let a363 = 363 let a364 = 364 let a365 = 365 let a366 = 366 let a367 = 367 let a368 = 368 let a369 = 369
    let a370 = 370 let a371 = 371 let a372 = 372 let a373 = 373 let a374 = 374 let a375 = 375 let a376 = 376 let a377 = 377 let a378 = 378 let a379 = 379
    let a380 = 380 let a381 = 381 let a382 = 382 let a383 = 383 let a384 = 384 let a385 = 385 let a386 = 386 let a387 = 387 let a388 = 388 let a389 = 389
    let a390 = 390 let a391 = 391 let a392 = 392 let a393 = 393 let a394 = 394 let a395 = 395 let a396 = 396 let a397 = 397 let a398 = 398 let a399 = 399
    return a0 + a255 + a399
}
let r2 = four_hundred_locals()
assert_eq(r2, 654, "400 locals")

// ============================================================
// SECTION 3: Wide with if control flow
// ============================================================
print("--- Section 3: Wide + if ---")

fn wide_with_if() {
    let x0 = 0 let x1 = 1 let x2 = 2 let x3 = 3 let x4 = 4 let x5 = 5 let x6 = 6 let x7 = 7 let x8 = 8 let x9 = 9
    let x10 = 10 let x11 = 11 let x12 = 12 let x13 = 13 let x14 = 14 let x15 = 15 let x16 = 16 let x17 = 17 let x18 = 18 let x19 = 19
    let x20 = 20 let x21 = 21 let x22 = 22 let x23 = 23 let x24 = 24 let x25 = 25 let x26 = 26 let x27 = 27 let x28 = 28 let x29 = 29
    let x30 = 30 let x31 = 31 let x32 = 32 let x33 = 33 let x34 = 34 let x35 = 35 let x36 = 36 let x37 = 37 let x38 = 38 let x39 = 39
    let x40 = 40 let x41 = 41 let x42 = 42 let x43 = 43 let x44 = 44 let x45 = 45 let x46 = 46 let x47 = 47 let x48 = 48 let x49 = 49
    let x50 = 50 let x51 = 51 let x52 = 52 let x53 = 53 let x54 = 54 let x55 = 55 let x56 = 56 let x57 = 57 let x58 = 58 let x59 = 59
    let x60 = 60 let x61 = 61 let x62 = 62 let x63 = 63 let x64 = 64 let x65 = 65 let x66 = 66 let x67 = 67 let x68 = 68 let x69 = 69
    let x70 = 70 let x71 = 71 let x72 = 72 let x73 = 73 let x74 = 74 let x75 = 75 let x76 = 76 let x77 = 77 let x78 = 78 let x79 = 79
    let x80 = 80 let x81 = 81 let x82 = 82 let x83 = 83 let x84 = 84 let x85 = 85 let x86 = 86 let x87 = 87 let x88 = 88 let x89 = 89
    let x90 = 90 let x91 = 91 let x92 = 92 let x93 = 93 let x94 = 94 let x95 = 95 let x96 = 96 let x97 = 97 let x98 = 98 let x99 = 99
    let x100 = 100 let x101 = 101 let x102 = 102 let x103 = 103 let x104 = 104 let x105 = 105 let x106 = 106 let x107 = 107 let x108 = 108 let x109 = 109
    let x110 = 110 let x111 = 111 let x112 = 112 let x113 = 113 let x114 = 114 let x115 = 115 let x116 = 116 let x117 = 117 let x118 = 118 let x119 = 119
    let x120 = 120 let x121 = 121 let x122 = 122 let x123 = 123 let x124 = 124 let x125 = 125 let x126 = 126 let x127 = 127 let x128 = 128 let x129 = 129
    let x130 = 130 let x131 = 131 let x132 = 132 let x133 = 133 let x134 = 134 let x135 = 135 let x136 = 136 let x137 = 137 let x138 = 138 let x139 = 139
    let x140 = 140 let x141 = 141 let x142 = 142 let x143 = 143 let x144 = 144 let x145 = 145 let x146 = 146 let x147 = 147 let x148 = 148 let x149 = 149
    let x150 = 150 let x151 = 151 let x152 = 152 let x153 = 153 let x154 = 154 let x155 = 155 let x156 = 156 let x157 = 157 let x158 = 158 let x159 = 159
    let x160 = 160 let x161 = 161 let x162 = 162 let x163 = 163 let x164 = 164 let x165 = 165 let x166 = 166 let x167 = 167 let x168 = 168 let x169 = 169
    let x170 = 170 let x171 = 171 let x172 = 172 let x173 = 173 let x174 = 174 let x175 = 175 let x176 = 176 let x177 = 177 let x178 = 178 let x179 = 179
    let x180 = 180 let x181 = 181 let x182 = 182 let x183 = 183 let x184 = 184 let x185 = 185 let x186 = 186 let x187 = 187 let x188 = 188 let x189 = 189
    let x190 = 190 let x191 = 191 let x192 = 192 let x193 = 193 let x194 = 194 let x195 = 195 let x196 = 196 let x197 = 197 let x198 = 198 let x199 = 199
    let x200 = 200 let x201 = 201 let x202 = 202 let x203 = 203 let x204 = 204 let x205 = 205 let x206 = 206 let x207 = 207 let x208 = 208 let x209 = 209
    let x210 = 210 let x211 = 211 let x212 = 212 let x213 = 213 let x214 = 214 let x215 = 215 let x216 = 216 let x217 = 217 let x218 = 218 let x219 = 219
    let x220 = 220 let x221 = 221 let x222 = 222 let x223 = 223 let x224 = 224 let x225 = 225 let x226 = 226 let x227 = 227 let x228 = 228 let x229 = 229
    let x230 = 230 let x231 = 231 let x232 = 232 let x233 = 233 let x234 = 234 let x235 = 235 let x236 = 236 let x237 = 237 let x238 = 238 let x239 = 239
    let x240 = 240 let x241 = 241 let x242 = 242 let x243 = 243 let x244 = 244 let x245 = 245 let x246 = 246 let x247 = 247 let x248 = 248 let x249 = 249
    let x250 = 250 let x251 = 251 let x252 = 252 let x253 = 253 let x254 = 254 let x255 = 255 let x256 = 256 let x257 = 257 let x258 = 258 let x259 = 259
    let x260 = 260 let x261 = 261 let x262 = 262 let x263 = 263 let x264 = 264 let x265 = 265 let x266 = 266 let x267 = 267 let x268 = 268 let x269 = 269
    let x270 = 270 let x271 = 271 let x272 = 272 let x273 = 273 let x274 = 274 let x275 = 275 let x276 = 276 let x277 = 277 let x278 = 278 let x279 = 279
    let x280 = 280 let x281 = 281 let x282 = 282 let x283 = 283 let x284 = 284 let x285 = 285 let x286 = 286 let x287 = 287 let x288 = 288 let x289 = 289
    let x290 = 290 let x291 = 291 let x292 = 292 let x293 = 293 let x294 = 294 let x295 = 295 let x296 = 296 let x297 = 297 let x298 = 298 let x299 = 299
    let x300 = 300
    // Control flow using high-index registers
    if (x300 > 299) {
        return x300 + x255 + x0
    }
    return -1
}
let r3 = wide_with_if()
assert_eq(r3, 555, "wide + if")

// ============================================================
// SECTION 4: Wide register assignment and arithmetic
// ============================================================
print("--- Section 4: Wide assignment ---")

fn wide_assign() {
    let w0 = 0 let w1 = 1 let w2 = 2 let w3 = 3 let w4 = 4 let w5 = 5 let w6 = 6 let w7 = 7 let w8 = 8 let w9 = 9
    let w10 = 10 let w11 = 11 let w12 = 12 let w13 = 13 let w14 = 14 let w15 = 15 let w16 = 16 let w17 = 17 let w18 = 18 let w19 = 19
    let w20 = 20 let w21 = 21 let w22 = 22 let w23 = 23 let w24 = 24 let w25 = 25 let w26 = 26 let w27 = 27 let w28 = 28 let w29 = 29
    let w30 = 30 let w31 = 31 let w32 = 32 let w33 = 33 let w34 = 34 let w35 = 35 let w36 = 36 let w37 = 37 let w38 = 38 let w39 = 39
    let w40 = 40 let w41 = 41 let w42 = 42 let w43 = 43 let w44 = 44 let w45 = 45 let w46 = 46 let w47 = 47 let w48 = 48 let w49 = 49
    let w50 = 50 let w51 = 51 let w52 = 52 let w53 = 53 let w54 = 54 let w55 = 55 let w56 = 56 let w57 = 57 let w58 = 58 let w59 = 59
    let w60 = 60 let w61 = 61 let w62 = 62 let w63 = 63 let w64 = 64 let w65 = 65 let w66 = 66 let w67 = 67 let w68 = 68 let w69 = 69
    let w70 = 70 let w71 = 71 let w72 = 72 let w73 = 73 let w74 = 74 let w75 = 75 let w76 = 76 let w77 = 77 let w78 = 78 let w79 = 79
    let w80 = 80 let w81 = 81 let w82 = 82 let w83 = 83 let w84 = 84 let w85 = 85 let w86 = 86 let w87 = 87 let w88 = 88 let w89 = 89
    let w90 = 90 let w91 = 91 let w92 = 92 let w93 = 93 let w94 = 94 let w95 = 95 let w96 = 96 let w97 = 97 let w98 = 98 let w99 = 99
    let w100 = 100 let w101 = 101 let w102 = 102 let w103 = 103 let w104 = 104 let w105 = 105 let w106 = 106 let w107 = 107 let w108 = 108 let w109 = 109
    let w110 = 110 let w111 = 111 let w112 = 112 let w113 = 113 let w114 = 114 let w115 = 115 let w116 = 116 let w117 = 117 let w118 = 118 let w119 = 119
    let w120 = 120 let w121 = 121 let w122 = 122 let w123 = 123 let w124 = 124 let w125 = 125 let w126 = 126 let w127 = 127 let w128 = 128 let w129 = 129
    let w130 = 130 let w131 = 131 let w132 = 132 let w133 = 133 let w134 = 134 let w135 = 135 let w136 = 136 let w137 = 137 let w138 = 138 let w139 = 139
    let w140 = 140 let w141 = 141 let w142 = 142 let w143 = 143 let w144 = 144 let w145 = 145 let w146 = 146 let w147 = 147 let w148 = 148 let w149 = 149
    let w150 = 150 let w151 = 151 let w152 = 152 let w153 = 153 let w154 = 154 let w155 = 155 let w156 = 156 let w157 = 157 let w158 = 158 let w159 = 159
    let w160 = 160 let w161 = 161 let w162 = 162 let w163 = 163 let w164 = 164 let w165 = 165 let w166 = 166 let w167 = 167 let w168 = 168 let w169 = 169
    let w170 = 170 let w171 = 171 let w172 = 172 let w173 = 173 let w174 = 174 let w175 = 175 let w176 = 176 let w177 = 177 let w178 = 178 let w179 = 179
    let w180 = 180 let w181 = 181 let w182 = 182 let w183 = 183 let w184 = 184 let w185 = 185 let w186 = 186 let w187 = 187 let w188 = 188 let w189 = 189
    let w190 = 190 let w191 = 191 let w192 = 192 let w193 = 193 let w194 = 194 let w195 = 195 let w196 = 196 let w197 = 197 let w198 = 198 let w199 = 199
    let w200 = 200 let w201 = 201 let w202 = 202 let w203 = 203 let w204 = 204 let w205 = 205 let w206 = 206 let w207 = 207 let w208 = 208 let w209 = 209
    let w210 = 210 let w211 = 211 let w212 = 212 let w213 = 213 let w214 = 214 let w215 = 215 let w216 = 216 let w217 = 217 let w218 = 218 let w219 = 219
    let w220 = 220 let w221 = 221 let w222 = 222 let w223 = 223 let w224 = 224 let w225 = 225 let w226 = 226 let w227 = 227 let w228 = 228 let w229 = 229
    let w230 = 230 let w231 = 231 let w232 = 232 let w233 = 233 let w234 = 234 let w235 = 235 let w236 = 236 let w237 = 237 let w238 = 238 let w239 = 239
    let w240 = 240 let w241 = 241 let w242 = 242 let w243 = 243 let w244 = 244 let w245 = 245 let w246 = 246 let w247 = 247 let w248 = 248 let w249 = 249
    let w250 = 250 let w251 = 251 let w252 = 252 let w253 = 253 let w254 = 254 let w255 = 255 let w256 = 256 let w257 = 257 let w258 = 258 let w259 = 259
    let w260 = 260 let w261 = 261 let w262 = 262 let w263 = 263 let w264 = 264 let w265 = 265 let w266 = 266 let w267 = 267 let w268 = 268 let w269 = 269
    let w270 = 270 let w271 = 271 let w272 = 272 let w273 = 273 let w274 = 274 let w275 = 275 let w276 = 276 let w277 = 277 let w278 = 278 let w279 = 279
    let w280 = 280 let w281 = 281 let w282 = 282 let w283 = 283 let w284 = 284 let w285 = 285 let w286 = 286 let w287 = 287 let w288 = 288 let w289 = 289
    let w290 = 290 let w291 = 291 let w292 = 292 let w293 = 293 let w294 = 294 let w295 = 295 let w296 = 296 let w297 = 297 let w298 = 298 let w299 = 299
    // Simple wide assignment and arithmetic (no loops)
    let x = 100
    x = x + 50
    x = x * 2
    return x
}
let r4 = wide_assign()
assert_eq(r4, 300, "wide assign")

print("")
print("=== Results ===")
print("Passed: " + to_string(pass_count))
print("Failed: " + to_string(fail_count))
if (fail_count == 0) {
    print("ALL TESTS PASSED!")
}
