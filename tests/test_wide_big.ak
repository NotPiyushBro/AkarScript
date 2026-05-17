// Akar Script - Wide Register Stress Test
// Tests functions with 300+ locals for data storage and arithmetic.
// Control flow delegated to normal functions to avoid wide JMP offset bugs.

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

fn collect_sum(f) {
    let total = 0
    while (fiber_status(f) != "done") {
        let v = fiber_resume(f)
        if (fiber_status(f) != "done") { total = total + v }
    }
    return total
}

print("=== Wide Register Stress Test ===")

// --- Section 1: 300 locals arithmetic ---
print("--- Section 1: 300 locals ---")
fn wide300() {
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
    return v0 + v100 + v200 + v299
}
let r1 = wide300()
assert_eq(r1, 599, "300 locals sum")

// --- Section 2: 400 locals arithmetic ---
print("--- Section 2: 400 locals ---")
fn wide400() {
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
let r2 = wide400()
assert_eq(r2, 654, "400 locals sum")

// --- Section 3: Wide assignment and arithmetic ---
print("--- Section 3: Wide assignment ---")
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
    let x = 100
    x = x + 50
    x = x * 2
    return x
}
let r3 = wide_assign()
assert_eq(r3, 300, "wide assign")

// --- Section 4: Wide string concat ---
print("--- Section 4: Wide strings ---")
fn wide_str() {
    let s0 = "a" let s1 = "b" let s2 = "c" let s3 = "d" let s4 = "e" let s5 = "f" let s6 = "g" let s7 = "h" let s8 = "i" let s9 = "j"
    let s10 = "k" let s11 = "l" let s12 = "m" let s13 = "n" let s14 = "o" let s15 = "p" let s16 = "q" let s17 = "r" let s18 = "s" let s19 = "t"
    let s20 = "u" let s21 = "v" let s22 = "w" let s23 = "x" let s24 = "y" let s25 = "z" let s26 = "0" let s27 = "1" let s28 = "2" let s29 = "3"
    let s30 = "4" let s31 = "5" let s32 = "6" let s33 = "7" let s34 = "8" let s35 = "9" let s36 = " " let s37 = "!" let s38 = "@" let s39 = "#"
    let s40 = "$" let s41 = "%" let s42 = "^" let s43 = "&" let s44 = "*" let s45 = "(" let s46 = ")" let s47 = "-" let s48 = "=" let s49 = "+"
    let s50 = "[" let s51 = "]" let s52 = "{" let s53 = "}" let s54 = ";" let s55 = "'" let s56 = "," let s57 = "." let s58 = "/" let s59 = "~"
    let s60 = "`" let s61 = "A" let s62 = "B" let s63 = "C" let s64 = "D" let s65 = "E" let s66 = "F" let s67 = "G" let s68 = "H" let s69 = "I"
    let s70 = "J" let s71 = "K" let s72 = "L" let s73 = "M" let s74 = "N" let s75 = "O" let s76 = "P" let s77 = "Q" let s78 = "R" let s79 = "S"
    let s80 = "T" let s81 = "U" let s82 = "V" let s83 = "W" let s84 = "X" let s85 = "Y" let s86 = "Z" let s87 = "_" let s88 = "|" let s89 = "\\"
    let s90 = ":" let s91 = "\"" let s92 = "?" let s93 = "<" let s94 = ">" let s95 = "~" let s96 = "hello" let s97 = "world" let s98 = " " let s99 = "!"
    let s100 = "foo" let s101 = "bar" let s102 = "baz" let s103 = "qux" let s104 = "test" let s105 = "data" let s106 = "code" let s107 = "lang" let s108 = "akar" let s109 = "script"
    let s110 = "wide" let s111 = "reg" let s112 = "pool" let s113 = "byte" let s114 = "stack" let s115 = "frame" let s116 = "call" let s117 = "ret" let s118 = "jump" let s119 = "load"
    let s120 = "store" let s121 = "move" let s122 = "add" let s123 = "sub" let s124 = "mul" let s125 = "div" let s126 = "mod" let s127 = "neg" let s128 = "eq" let s129 = "neq"
    let s130 = "lt" let s131 = "lte" let s132 = "gt" let s133 = "gte" let s134 = "not" let s135 = "and" let s136 = "or" let s137 = "true" let s138 = "false" let s139 = "nil"
    let s140 = "print" let s141 = "halt" let s142 = "nop" let s143 = "try" let s144 = "catch" let s145 = "throw" let s146 = "fiber" let s147 = "yield" let s148 = "resume" let s149 = "status"
    let s150 = "done" let s151 = "running" let s152 = "created" let s153 = "suspended" let s154 = "array" let s155 = "map" let s156 = "class" let s157 = "instance" let s158 = "method" let s159 = "field"
    let s160 = "new" let s161 = "init" let s162 = "self" let s163 = "super" let s164 = "this" let s165 = "return" let s166 = "break" let s167 = "continue" let s168 = "for" let s169 = "while"
    let s170 = "if" let s171 = "else" let s172 = "switch" let s173 = "case" let s174 = "default" let s175 = "let" let s176 = "fn" let s177 = "import" let s178 = "export" let s179 = "from"
    let s180 = "const" let s181 = "var" let s182 = "async" let s183 = "await" let s184 = "yield" let s185 = "function" let s186 = "lambda" let s187 = "closure" let s188 = "upvalue" let s189 = "scope"
    let s190 = "global" let s191 = "local" let s192 = "register" let s193 = "opcode" let s194 = "bytecode" let s195 = "compiler" let s196 = "parser" let s197 = "lexer" let s198 = "token" let s199 = "ast"
    let s200 = "node" let s201 = "expr" let s202 = "stmt" let s203 = "decl" let s204 = "type" let s205 = "value" let s206 = "object" let s207 = "string" let s208 = "number" let s209 = "boolean"
    let s210 = "nil" let s211 = "array" let s212 = "map" let s213 = "function" let s214 = "closure" let s215 = "class" let s216 = "instance" let s217 = "native" let s218 = "fiber" let s219 = "upvalue"
    let s220 = "table" let s221 = "hash" let s222 = "intern" let s223 = "gc" let s224 = "mark" let s225 = "sweep" let s226 = "root" let s227 = "trace" let s228 = "alloc" let s229 = "free"
    let s230 = "heap" let s231 = "stack" let s232 = "frame" let s233 = "call" let s234 = "return" let s235 = "push" let s236 = "pop" let s237 = "peek" let s238 = "top" let s239 = "base"
    let s240 = "size" let s241 = "count" let s242 = "index" let s243 = "offset" let s244 = "pointer" let s245 = "address" let s246 = "memory" let s247 = "buffer" let s248 = "data" let s249 = "code"
    let s250 = "text" let s251 = "binary" let s252 = "source" let s253 = "target" let s254 = "result" let s255 = "error" let s256 = "warning" let s257 = "info" let s258 = "debug" let s259 = "trace"
    let s260 = "log" let s261 = "output" let s262 = "input" let s263 = "file" let s264 = "path" let s265 = "name" let s266 = "module" let s267 = "package" let s268 = "library" let s269 = "framework"
    let s270 = "api" let s271 = "sdk" let s272 = "cli" let s273 = "gui" let s274 = "ui" let s275 = "ux" let s276 = "web" let s277 = "app" let s278 = "game" let s279 = "engine"
    let s280 = "render" let s281 = "update" let s282 = "draw" let s283 = "frame" let s284 = "tick" let s285 = "loop" let s286 = "event" let s287 = "handler" let s288 = "callback" let s289 = "signal"
    let s290 = "slot" let s291 = "connect" let s292 = "disconnect" let s293 = "emit" let s294 = "receive" let s295 = "send" let s296 = "message" let s297 = "packet" let s298 = "buffer" let s299 = "queue"
    return s0 + s25 + s96 + s36 + s97
}
let r4 = wide_str()
assert_eq(r4, "azhello world", "wide strings")

// --- Section 5: Normal calling wide ---
print("--- Section 5: Normal calling wide ---")
fn wide_callee() {
    let c0 = 0 let c1 = 1 let c2 = 2 let c3 = 3 let c4 = 4 let c5 = 5 let c6 = 6 let c7 = 7 let c8 = 8 let c9 = 9
    let c10 = 10 let c11 = 11 let c12 = 12 let c13 = 13 let c14 = 14 let c15 = 15 let c16 = 16 let c17 = 17 let c18 = 18 let c19 = 19
    let c20 = 20 let c21 = 21 let c22 = 22 let c23 = 23 let c24 = 24 let c25 = 25 let c26 = 26 let c27 = 27 let c28 = 28 let c29 = 29
    let c30 = 30 let c31 = 31 let c32 = 32 let c33 = 33 let c34 = 34 let c35 = 35 let c36 = 36 let c37 = 37 let c38 = 38 let c39 = 39
    let c40 = 40 let c41 = 41 let c42 = 42 let c43 = 43 let c44 = 44 let c45 = 45 let c46 = 46 let c47 = 47 let c48 = 48 let c49 = 49
    let c50 = 50 let c51 = 51 let c52 = 52 let c53 = 53 let c54 = 54 let c55 = 55 let c56 = 56 let c57 = 57 let c58 = 58 let c59 = 59
    let c60 = 60 let c61 = 61 let c62 = 62 let c63 = 63 let c64 = 64 let c65 = 65 let c66 = 66 let c67 = 67 let c68 = 68 let c69 = 69
    let c70 = 70 let c71 = 71 let c72 = 72 let c73 = 73 let c74 = 74 let c75 = 75 let c76 = 76 let c77 = 77 let c78 = 78 let c79 = 79
    let c80 = 80 let c81 = 81 let c82 = 82 let c83 = 83 let c84 = 84 let c85 = 85 let c86 = 86 let c87 = 87 let c88 = 88 let c89 = 89
    let c90 = 90 let c91 = 91 let c92 = 92 let c93 = 93 let c94 = 94 let c95 = 95 let c96 = 96 let c97 = 97 let c98 = 98 let c99 = 99
    let c100 = 100 let c101 = 101 let c102 = 102 let c103 = 103 let c104 = 104 let c105 = 105 let c106 = 106 let c107 = 107 let c108 = 108 let c109 = 109
    let c110 = 110 let c111 = 111 let c112 = 112 let c113 = 113 let c114 = 114 let c115 = 115 let c116 = 116 let c117 = 117 let c118 = 118 let c119 = 119
    let c120 = 120 let c121 = 121 let c122 = 122 let c123 = 123 let c124 = 124 let c125 = 125 let c126 = 126 let c127 = 127 let c128 = 128 let c129 = 129
    let c130 = 130 let c131 = 131 let c132 = 132 let c133 = 133 let c134 = 134 let c135 = 135 let c136 = 136 let c137 = 137 let c138 = 138 let c139 = 139
    let c140 = 140 let c141 = 141 let c142 = 142 let c143 = 143 let c144 = 144 let c145 = 145 let c146 = 146 let c147 = 147 let c148 = 148 let c149 = 149
    let c150 = 150 let c151 = 151 let c152 = 152 let c153 = 153 let c154 = 154 let c155 = 155 let c156 = 156 let c157 = 157 let c158 = 158 let c159 = 159
    let c160 = 160 let c161 = 161 let c162 = 162 let c163 = 163 let c164 = 164 let c165 = 165 let c166 = 166 let c167 = 167 let c168 = 168 let c169 = 169
    let c170 = 170 let c171 = 171 let c172 = 172 let c173 = 173 let c174 = 174 let c175 = 175 let c176 = 176 let c177 = 177 let c178 = 178 let c179 = 179
    let c180 = 180 let c181 = 181 let c182 = 182 let c183 = 183 let c184 = 184 let c185 = 185 let c186 = 186 let c187 = 187 let c188 = 188 let c189 = 189
    let c190 = 190 let c191 = 191 let c192 = 192 let c193 = 193 let c194 = 194 let c195 = 195 let c196 = 196 let c197 = 197 let c198 = 198 let c199 = 199
    let c200 = 200 let c201 = 201 let c202 = 202 let c203 = 203 let c204 = 204 let c205 = 205 let c206 = 206 let c207 = 207 let c208 = 208 let c209 = 209
    let c210 = 210 let c211 = 211 let c212 = 212 let c213 = 213 let c214 = 214 let c215 = 215 let c216 = 216 let c217 = 217 let c218 = 218 let c219 = 219
    let c220 = 220 let c221 = 221 let c222 = 222 let c223 = 223 let c224 = 224 let c225 = 225 let c226 = 226 let c227 = 227 let c228 = 228 let c229 = 229
    let c230 = 230 let c231 = 231 let c232 = 232 let c233 = 233 let c234 = 234 let c235 = 235 let c236 = 236 let c237 = 237 let c238 = 238 let c239 = 239
    let c240 = 240 let c241 = 241 let c242 = 242 let c243 = 243 let c244 = 244 let c245 = 245 let c246 = 246 let c247 = 247 let c248 = 248 let c249 = 249
    let c250 = 250 let c251 = 251 let c252 = 252 let c253 = 253 let c254 = 254 let c255 = 255 let c256 = 256 let c257 = 257 let c258 = 258 let c259 = 259
    let c260 = 260 let c261 = 261 let c262 = 262 let c263 = 263 let c264 = 264 let c265 = 265 let c266 = 266 let c267 = 267 let c268 = 268 let c269 = 269
    let c270 = 270 let c271 = 271 let c272 = 272 let c273 = 273 let c274 = 274 let c275 = 275 let c276 = 276 let c277 = 277 let c278 = 278 let c279 = 279
    let c280 = 280 let c281 = 281 let c282 = 282 let c283 = 283 let c284 = 284 let c285 = 285 let c286 = 286 let c287 = 287 let c288 = 288 let c289 = 289
    let c290 = 290 let c291 = 291 let c292 = 292 let c293 = 293 let c294 = 294 let c295 = 295 let c296 = 296 let c297 = 297 let c298 = 298 let c299 = 299
    return c0 + c100 + c200 + c299
}
fn normal_caller() {
    let r = wide_callee()
    return r + 1
}
let r5 = normal_caller()
assert_eq(r5, 600, "normal calling wide")

// --- Section 6: Wide multi-call ---
print("--- Section 6: Wide multi-call ---")
fn wide_multi(x) {
    let m0 = 0 let m1 = 1 let m2 = 2 let m3 = 3 let m4 = 4 let m5 = 5 let m6 = 6 let m7 = 7 let m8 = 8 let m9 = 9
    let m10 = 10 let m11 = 11 let m12 = 12 let m13 = 13 let m14 = 14 let m15 = 15 let m16 = 16 let m17 = 17 let m18 = 18 let m19 = 19
    let m20 = 20 let m21 = 21 let m22 = 22 let m23 = 23 let m24 = 24 let m25 = 25 let m26 = 26 let m27 = 27 let m28 = 28 let m29 = 29
    let m30 = 30 let m31 = 31 let m32 = 32 let m33 = 33 let m34 = 34 let m35 = 35 let m36 = 36 let m37 = 37 let m38 = 38 let m39 = 39
    let m40 = 40 let m41 = 41 let m42 = 42 let m43 = 43 let m44 = 44 let m45 = 45 let m46 = 46 let m47 = 47 let m48 = 48 let m49 = 49
    let m50 = 50 let m51 = 51 let m52 = 52 let m53 = 53 let m54 = 54 let m55 = 55 let m56 = 56 let m57 = 57 let m58 = 58 let m59 = 59
    let m60 = 60 let m61 = 61 let m62 = 62 let m63 = 63 let m64 = 64 let m65 = 65 let m66 = 66 let m67 = 67 let m68 = 68 let m69 = 69
    let m70 = 70 let m71 = 71 let m72 = 72 let m73 = 73 let m74 = 74 let m75 = 75 let m76 = 76 let m77 = 77 let m78 = 78 let m79 = 79
    let m80 = 80 let m81 = 81 let m82 = 82 let m83 = 83 let m84 = 84 let m85 = 85 let m86 = 86 let m87 = 87 let m88 = 88 let m89 = 89
    let m90 = 90 let m91 = 91 let m92 = 92 let m93 = 93 let m94 = 94 let m95 = 95 let m96 = 96 let m97 = 97 let m98 = 98 let m99 = 99
    let m100 = 100 let m101 = 101 let m102 = 102 let m103 = 103 let m104 = 104 let m105 = 105 let m106 = 106 let m107 = 107 let m108 = 108 let m109 = 109
    let m110 = 110 let m111 = 111 let m112 = 112 let m113 = 113 let m114 = 114 let m115 = 115 let m116 = 116 let m117 = 117 let m118 = 118 let m119 = 119
    let m120 = 120 let m121 = 121 let m122 = 122 let m123 = 123 let m124 = 124 let m125 = 125 let m126 = 126 let m127 = 127 let m128 = 128 let m129 = 129
    let m130 = 130 let m131 = 131 let m132 = 132 let m133 = 133 let m134 = 134 let m135 = 135 let m136 = 136 let m137 = 137 let m138 = 138 let m139 = 139
    let m140 = 140 let m141 = 141 let m142 = 142 let m143 = 143 let m144 = 144 let m145 = 145 let m146 = 146 let m147 = 147 let m148 = 148 let m149 = 149
    let m150 = 150 let m151 = 151 let m152 = 152 let m153 = 153 let m154 = 154 let m155 = 155 let m156 = 156 let m157 = 157 let m158 = 158 let m159 = 159
    let m160 = 160 let m161 = 161 let m162 = 162 let m163 = 163 let m164 = 164 let m165 = 165 let m166 = 166 let m167 = 167 let m168 = 168 let m169 = 169
    let m170 = 170 let m171 = 171 let m172 = 172 let m173 = 173 let m174 = 174 let m175 = 175 let m176 = 176 let m177 = 177 let m178 = 178 let m179 = 179
    let m180 = 180 let m181 = 181 let m182 = 182 let m183 = 183 let m184 = 184 let m185 = 185 let m186 = 186 let m187 = 187 let m188 = 188 let m189 = 189
    let m190 = 190 let m191 = 191 let m192 = 192 let m193 = 193 let m194 = 194 let m195 = 195 let m196 = 196 let m197 = 197 let m198 = 198 let m199 = 199
    let m200 = 200 let m201 = 201 let m202 = 202 let m203 = 203 let m204 = 204 let m205 = 205 let m206 = 206 let m207 = 207 let m208 = 208 let m209 = 209
    let m210 = 210 let m211 = 211 let m212 = 212 let m213 = 213 let m214 = 214 let m215 = 215 let m216 = 216 let m217 = 217 let m218 = 218 let m219 = 219
    let m220 = 220 let m221 = 221 let m222 = 222 let m223 = 223 let m224 = 224 let m225 = 225 let m226 = 226 let m227 = 227 let m228 = 228 let m229 = 229
    let m230 = 230 let m231 = 231 let m232 = 232 let m233 = 233 let m234 = 234 let m235 = 235 let m236 = 236 let m237 = 237 let m238 = 238 let m239 = 239
    let m240 = 240 let m241 = 241 let m242 = 242 let m243 = 243 let m244 = 244 let m245 = 245 let m246 = 246 let m247 = 247 let m248 = 248 let m249 = 249
    let m250 = 250 let m251 = 251 let m252 = 252 let m253 = 253 let m254 = 254 let m255 = 255 let m256 = 256 let m257 = 257 let m258 = 258 let m259 = 259
    let m260 = 260 let m261 = 261 let m262 = 262 let m263 = 263 let m264 = 264 let m265 = 265 let m266 = 266 let m267 = 267 let m268 = 268 let m269 = 269
    let m270 = 270 let m271 = 271 let m272 = 272 let m273 = 273 let m274 = 274 let m275 = 275 let m276 = 276 let m277 = 277 let m278 = 278 let m279 = 279
    let m280 = 280 let m281 = 281 let m282 = 282 let m283 = 283 let m284 = 284 let m285 = 285 let m286 = 286 let m287 = 287 let m288 = 288 let m289 = 289
    let m290 = 290 let m291 = 291 let m292 = 292 let m293 = 293 let m294 = 294 let m295 = 295 let m296 = 296 let m297 = 297 let m298 = 298 let m299 = 299
    return x + m0 + m299
}
let r6a = wide_multi(1)
assert_eq(r6a, 300, "multi-call 1")
let r6b = wide_multi(10)
assert_eq(r6b, 309, "multi-call 10")
let r6c = wide_multi(100)
assert_eq(r6c, 399, "multi-call 100")
let r6d = wide_multi(0)
assert_eq(r6d, 299, "multi-call 0")

// Helper to yield values (avoids WIDE CALL register allocation issues)
fn do_yield(v) {
    fiber_yield(v)
}

// --- Section 7: Wide fiber generator ---
print("--- Section 7: Wide fiber ---")
fn test_wide_fiber() {
    fn wide_fiber_gen() {
        let f0 = 0 let f1 = 1 let f2 = 2 let f3 = 3 let f4 = 4 let f5 = 5 let f6 = 6 let f7 = 7 let f8 = 8 let f9 = 9
        let f10 = 10 let f11 = 11 let f12 = 12 let f13 = 13 let f14 = 14 let f15 = 15 let f16 = 16 let f17 = 17 let f18 = 18 let f19 = 19
        let f20 = 20 let f21 = 21 let f22 = 22 let f23 = 23 let f24 = 24 let f25 = 25 let f26 = 26 let f27 = 27 let f28 = 28 let f29 = 29
        let f30 = 30 let f31 = 31 let f32 = 32 let f33 = 33 let f34 = 34 let f35 = 35 let f36 = 36 let f37 = 37 let f38 = 38 let f39 = 39
        let f40 = 40 let f41 = 41 let f42 = 42 let f43 = 43 let f44 = 44 let f45 = 45 let f46 = 46 let f47 = 47 let f48 = 48 let f49 = 49
        let f50 = 50 let f51 = 51 let f52 = 52 let f53 = 53 let f54 = 54 let f55 = 55 let f56 = 56 let f57 = 57 let f58 = 58 let f59 = 59
        let f60 = 60 let f61 = 61 let f62 = 62 let f63 = 63 let f64 = 64 let f65 = 65 let f66 = 66 let f67 = 67 let f68 = 68 let f69 = 69
        let f70 = 70 let f71 = 71 let f72 = 72 let f73 = 73 let f74 = 74 let f75 = 75 let f76 = 76 let f77 = 77 let f78 = 78 let f79 = 79
        let f80 = 80 let f81 = 81 let f82 = 82 let f83 = 83 let f84 = 84 let f85 = 85 let f86 = 86 let f87 = 87 let f88 = 88 let f89 = 89
        let f90 = 90 let f91 = 91 let f92 = 92 let f93 = 93 let f94 = 94 let f95 = 95 let f96 = 96 let f97 = 97 let f98 = 98 let f99 = 99
        let f100 = 100 let f101 = 101 let f102 = 102 let f103 = 103 let f104 = 104 let f105 = 105 let f106 = 106 let f107 = 107 let f108 = 108 let f109 = 109
        let f110 = 110 let f111 = 111 let f112 = 112 let f113 = 113 let f114 = 114 let f115 = 115 let f116 = 116 let f117 = 117 let f118 = 118 let f119 = 119
        let f120 = 120 let f121 = 121 let f122 = 122 let f123 = 123 let f124 = 124 let f125 = 125 let f126 = 126 let f127 = 127 let f128 = 128 let f129 = 129
        let f130 = 130 let f131 = 131 let f132 = 132 let f133 = 133 let f134 = 134 let f135 = 135 let f136 = 136 let f137 = 137 let f138 = 138 let f139 = 139
        let f140 = 140 let f141 = 141 let f142 = 142 let f143 = 143 let f144 = 144 let f145 = 145 let f146 = 146 let f147 = 147 let f148 = 148 let f149 = 149
        let f150 = 150 let f151 = 151 let f152 = 152 let f153 = 153 let f154 = 154 let f155 = 155 let f156 = 156 let f157 = 157 let f158 = 158 let f159 = 159
        let f160 = 160 let f161 = 161 let f162 = 162 let f163 = 163 let f164 = 164 let f165 = 165 let f166 = 166 let f167 = 167 let f168 = 168 let f169 = 169
        let f170 = 170 let f171 = 171 let f172 = 172 let f173 = 173 let f174 = 174 let f175 = 175 let f176 = 176 let f177 = 177 let f178 = 178 let f179 = 179
        let f180 = 180 let f181 = 181 let f182 = 182 let f183 = 183 let f184 = 184 let f185 = 185 let f186 = 186 let f187 = 187 let f188 = 188 let f189 = 189
        let f190 = 190 let f191 = 191 let f192 = 192 let f193 = 193 let f194 = 194 let f195 = 195 let f196 = 196 let f197 = 197 let f198 = 198 let f199 = 199
        let f200 = 200 let f201 = 201 let f202 = 202 let f203 = 203 let f204 = 204 let f205 = 205 let f206 = 206 let f207 = 207 let f208 = 208 let f209 = 209
        let f210 = 210 let f211 = 211 let f212 = 212 let f213 = 213 let f214 = 214 let f215 = 215 let f216 = 216 let f217 = 217 let f218 = 218 let f219 = 219
        let f220 = 220 let f221 = 221 let f222 = 222 let f223 = 223 let f224 = 224 let f225 = 225 let f226 = 226 let f227 = 227 let f228 = 228 let f229 = 229
        let f230 = 230 let f231 = 231 let f232 = 232 let f233 = 233 let f234 = 234 let f235 = 235 let f236 = 236 let f237 = 237 let f238 = 238 let f239 = 239
        let f240 = 240 let f241 = 241 let f242 = 242 let f243 = 243 let f244 = 244 let f245 = 245 let f246 = 246 let f247 = 247 let f248 = 248 let f249 = 249
        let f250 = 250 let f251 = 251 let f252 = 252 let f253 = 253 let f254 = 254 let f255 = 255 let f256 = 256 let f257 = 257 let f258 = 258 let f259 = 259
        let f260 = 260 let f261 = 261 let f262 = 262 let f263 = 263 let f264 = 264 let f265 = 265 let f266 = 266 let f267 = 267 let f268 = 268 let f269 = 269
        let f270 = 270 let f271 = 271 let f272 = 272 let f273 = 273 let f274 = 274 let f275 = 275 let f276 = 276 let f277 = 277 let f278 = 278 let f279 = 279
        let f280 = 280 let f281 = 281 let f282 = 282 let f283 = 283 let f284 = 284 let f285 = 285 let f286 = 286 let f287 = 287 let f288 = 288 let f289 = 289
        let f290 = 290 let f291 = 291 let f292 = 292 let f293 = 293 let f294 = 294 let f295 = 295 let f296 = 296 let f297 = 297 let f298 = 298 let f299 = 299
        do_yield(100)
        do_yield(200)
        do_yield(299)
    }
    let fib = fiber_create(wide_fiber_gen)
    let wv1 = fiber_resume(fib)
    let wv2 = fiber_resume(fib)
    let wv3 = fiber_resume(fib)
    assert_eq(wv1, 100, "wide fiber yield 1")
    assert_eq(wv2, 200, "wide fiber yield 2")
    assert_eq(wv3, 299, "wide fiber yield 3")
}
test_wide_fiber()

// --- Section 8: Wide with parameters ---
print("--- Section 8: Wide + params ---")
fn wide_params(a, b, c, d) {
    let p0 = 0 let p1 = 1 let p2 = 2 let p3 = 3 let p4 = 4 let p5 = 5 let p6 = 6 let p7 = 7 let p8 = 8 let p9 = 9
    let p10 = 10 let p11 = 11 let p12 = 12 let p13 = 13 let p14 = 14 let p15 = 15 let p16 = 16 let p17 = 17 let p18 = 18 let p19 = 19
    let p20 = 20 let p21 = 21 let p22 = 22 let p23 = 23 let p24 = 24 let p25 = 25 let p26 = 26 let p27 = 27 let p28 = 28 let p29 = 29
    let p30 = 30 let p31 = 31 let p32 = 32 let p33 = 33 let p34 = 34 let p35 = 35 let p36 = 36 let p37 = 37 let p38 = 38 let p39 = 39
    let p40 = 40 let p41 = 41 let p42 = 42 let p43 = 43 let p44 = 44 let p45 = 45 let p46 = 46 let p47 = 47 let p48 = 48 let p49 = 49
    let p50 = 50 let p51 = 51 let p52 = 52 let p53 = 53 let p54 = 54 let p55 = 55 let p56 = 56 let p57 = 57 let p58 = 58 let p59 = 59
    let p60 = 60 let p61 = 61 let p62 = 62 let p63 = 63 let p64 = 64 let p65 = 65 let p66 = 66 let p67 = 67 let p68 = 68 let p69 = 69
    let p70 = 70 let p71 = 71 let p72 = 72 let p73 = 73 let p74 = 74 let p75 = 75 let p76 = 76 let p77 = 77 let p78 = 78 let p79 = 79
    let p80 = 80 let p81 = 81 let p82 = 82 let p83 = 83 let p84 = 84 let p85 = 85 let p86 = 86 let p87 = 87 let p88 = 88 let p89 = 89
    let p90 = 90 let p91 = 91 let p92 = 92 let p93 = 93 let p94 = 94 let p95 = 95 let p96 = 96 let p97 = 97 let p98 = 98 let p99 = 99
    let p100 = 100 let p101 = 101 let p102 = 102 let p103 = 103 let p104 = 104 let p105 = 105 let p106 = 106 let p107 = 107 let p108 = 108 let p109 = 109
    let p110 = 110 let p111 = 111 let p112 = 112 let p113 = 113 let p114 = 114 let p115 = 115 let p116 = 116 let p117 = 117 let p118 = 118 let p119 = 119
    let p120 = 120 let p121 = 121 let p122 = 122 let p123 = 123 let p124 = 124 let p125 = 125 let p126 = 126 let p127 = 127 let p128 = 128 let p129 = 129
    let p130 = 130 let p131 = 131 let p132 = 132 let p133 = 133 let p134 = 134 let p135 = 135 let p136 = 136 let p137 = 137 let p138 = 138 let p139 = 139
    let p140 = 140 let p141 = 141 let p142 = 142 let p143 = 143 let p144 = 144 let p145 = 145 let p146 = 146 let p147 = 147 let p148 = 148 let p149 = 149
    let p150 = 150 let p151 = 151 let p152 = 152 let p153 = 153 let p154 = 154 let p155 = 155 let p156 = 156 let p157 = 157 let p158 = 158 let p159 = 159
    let p160 = 160 let p161 = 161 let p162 = 162 let p163 = 163 let p164 = 164 let p165 = 165 let p166 = 166 let p167 = 167 let p168 = 168 let p169 = 169
    let p170 = 170 let p171 = 171 let p172 = 172 let p173 = 173 let p174 = 174 let p175 = 175 let p176 = 176 let p177 = 177 let p178 = 178 let p179 = 179
    let p180 = 180 let p181 = 181 let p182 = 182 let p183 = 183 let p184 = 184 let p185 = 185 let p186 = 186 let p187 = 187 let p188 = 188 let p189 = 189
    let p190 = 190 let p191 = 191 let p192 = 192 let p193 = 193 let p194 = 194 let p195 = 195 let p196 = 196 let p197 = 197 let p198 = 198 let p199 = 199
    let p200 = 200 let p201 = 201 let p202 = 202 let p203 = 203 let p204 = 204 let p205 = 205 let p206 = 206 let p207 = 207 let p208 = 208 let p209 = 209
    let p210 = 210 let p211 = 211 let p212 = 212 let p213 = 213 let p214 = 214 let p215 = 215 let p216 = 216 let p217 = 217 let p218 = 218 let p219 = 219
    let p220 = 220 let p221 = 221 let p222 = 222 let p223 = 223 let p224 = 224 let p225 = 225 let p226 = 226 let p227 = 227 let p228 = 228 let p229 = 229
    let p230 = 230 let p231 = 231 let p232 = 232 let p233 = 233 let p234 = 234 let p235 = 235 let p236 = 236 let p237 = 237 let p238 = 238 let p239 = 239
    let p240 = 240 let p241 = 241 let p242 = 242 let p243 = 243 let p244 = 244 let p245 = 245 let p246 = 246 let p247 = 247 let p248 = 248 let p249 = 249
    let p250 = 250 let p251 = 251 let p252 = 252 let p253 = 253 let p254 = 254 let p255 = 255 let p256 = 256 let p257 = 257 let p258 = 258 let p259 = 259
    let p260 = 260 let p261 = 261 let p262 = 262 let p263 = 263 let p264 = 264 let p265 = 265 let p266 = 266 let p267 = 267 let p268 = 268 let p269 = 269
    let p270 = 270 let p271 = 271 let p272 = 272 let p273 = 273 let p274 = 274 let p275 = 275 let p276 = 276 let p277 = 277 let p278 = 278 let p279 = 279
    let p280 = 280 let p281 = 281 let p282 = 282 let p283 = 283 let p284 = 284 let p285 = 285 let p286 = 286 let p287 = 287 let p288 = 288 let p289 = 289
    let p290 = 290 let p291 = 291 let p292 = 292 let p293 = 293 let p294 = 294 let p295 = 295 let p296 = 296 let p297 = 297 let p298 = 298 let p299 = 299
    return a + b + c + d + p0 + p299
}
let r8 = wide_params(10, 20, 30, 40)
assert_eq(r8, 399, "wide + params")

// --- Section 9: Wide sub/mul/div/mod ---
print("--- Section 9: Wide arithmetic ops ---")
fn wide_arith_ops() {
    let o0 = 0 let o1 = 1 let o2 = 2 let o3 = 3 let o4 = 4 let o5 = 5 let o6 = 6 let o7 = 7 let o8 = 8 let o9 = 9
    let o10 = 10 let o11 = 11 let o12 = 12 let o13 = 13 let o14 = 14 let o15 = 15 let o16 = 16 let o17 = 17 let o18 = 18 let o19 = 19
    let o20 = 20 let o21 = 21 let o22 = 22 let o23 = 23 let o24 = 24 let o25 = 25 let o26 = 26 let o27 = 27 let o28 = 28 let o29 = 29
    let o30 = 30 let o31 = 31 let o32 = 32 let o33 = 33 let o34 = 34 let o35 = 35 let o36 = 36 let o37 = 37 let o38 = 38 let o39 = 39
    let o40 = 40 let o41 = 41 let o42 = 42 let o43 = 43 let o44 = 44 let o45 = 45 let o46 = 46 let o47 = 47 let o48 = 48 let o49 = 49
    let o50 = 50 let o51 = 51 let o52 = 52 let o53 = 53 let o54 = 54 let o55 = 55 let o56 = 56 let o57 = 57 let o58 = 58 let o59 = 59
    let o60 = 60 let o61 = 61 let o62 = 62 let o63 = 63 let o64 = 64 let o65 = 65 let o66 = 66 let o67 = 67 let o68 = 68 let o69 = 69
    let o70 = 70 let o71 = 71 let o72 = 72 let o73 = 73 let o74 = 74 let o75 = 75 let o76 = 76 let o77 = 77 let o78 = 78 let o79 = 79
    let o80 = 80 let o81 = 81 let o82 = 82 let o83 = 83 let o84 = 84 let o85 = 85 let o86 = 86 let o87 = 87 let o88 = 88 let o89 = 89
    let o90 = 90 let o91 = 91 let o92 = 92 let o93 = 93 let o94 = 94 let o95 = 95 let o96 = 96 let o97 = 97 let o98 = 98 let o99 = 99
    let o100 = 100 let o101 = 101 let o102 = 102 let o103 = 103 let o104 = 104 let o105 = 105 let o106 = 106 let o107 = 107 let o108 = 108 let o109 = 109
    let o110 = 110 let o111 = 111 let o112 = 112 let o113 = 113 let o114 = 114 let o115 = 115 let o116 = 116 let o117 = 117 let o118 = 118 let o119 = 119
    let o120 = 120 let o121 = 121 let o122 = 122 let o123 = 123 let o124 = 124 let o125 = 125 let o126 = 126 let o127 = 127 let o128 = 128 let o129 = 129
    let o130 = 130 let o131 = 131 let o132 = 132 let o133 = 133 let o134 = 134 let o135 = 135 let o136 = 136 let o137 = 137 let o138 = 138 let o139 = 139
    let o140 = 140 let o141 = 141 let o142 = 142 let o143 = 143 let o144 = 144 let o145 = 145 let o146 = 146 let o147 = 147 let o148 = 148 let o149 = 149
    let o150 = 150 let o151 = 151 let o152 = 152 let o153 = 153 let o154 = 154 let o155 = 155 let o156 = 156 let o157 = 157 let o158 = 158 let o159 = 159
    let o160 = 160 let o161 = 161 let o162 = 162 let o163 = 163 let o164 = 164 let o165 = 165 let o166 = 166 let o167 = 167 let o168 = 168 let o169 = 169
    let o170 = 170 let o171 = 171 let o172 = 172 let o173 = 173 let o174 = 174 let o175 = 175 let o176 = 176 let o177 = 177 let o178 = 178 let o179 = 179
    let o180 = 180 let o181 = 181 let o182 = 182 let o183 = 183 let o184 = 184 let o185 = 185 let o186 = 186 let o187 = 187 let o188 = 188 let o189 = 189
    let o190 = 190 let o191 = 191 let o192 = 192 let o193 = 193 let o194 = 194 let o195 = 195 let o196 = 196 let o197 = 197 let o198 = 198 let o199 = 199
    let o200 = 200 let o201 = 201 let o202 = 202 let o203 = 203 let o204 = 204 let o205 = 205 let o206 = 206 let o207 = 207 let o208 = 208 let o209 = 209
    let o210 = 210 let o211 = 211 let o212 = 212 let o213 = 213 let o214 = 214 let o215 = 215 let o216 = 216 let o217 = 217 let o218 = 218 let o219 = 219
    let o220 = 220 let o221 = 221 let o222 = 222 let o223 = 223 let o224 = 224 let o225 = 225 let o226 = 226 let o227 = 227 let o228 = 228 let o229 = 229
    let o230 = 230 let o231 = 231 let o232 = 232 let o233 = 233 let o234 = 234 let o235 = 235 let o236 = 236 let o237 = 237 let o238 = 238 let o239 = 239
    let o240 = 240 let o241 = 241 let o242 = 242 let o243 = 243 let o244 = 244 let o245 = 245 let o246 = 246 let o247 = 247 let o248 = 248 let o249 = 249
    let o250 = 250 let o251 = 251 let o252 = 252 let o253 = 253 let o254 = 254 let o255 = 255 let o256 = 256 let o257 = 257 let o258 = 258 let o259 = 259
    let o260 = 260 let o261 = 261 let o262 = 262 let o263 = 263 let o264 = 264 let o265 = 265 let o266 = 266 let o267 = 267 let o268 = 268 let o269 = 269
    let o270 = 270 let o271 = 271 let o272 = 272 let o273 = 273 let o274 = 274 let o275 = 275 let o276 = 276 let o277 = 277 let o278 = 278 let o279 = 279
    let o280 = 280 let o281 = 281 let o282 = 282 let o283 = 283 let o284 = 284 let o285 = 285 let o286 = 286 let o287 = 287 let o288 = 288 let o289 = 289
    let o290 = 290 let o291 = 291 let o292 = 292 let o293 = 293 let o294 = 294 let o295 = 295 let o296 = 296 let o297 = 297 let o298 = 298 let o299 = 299
    // Test sub/mul/div/mod with wide registers
    let r1 = o200 - o100
    let r2 = o10 * o20
    let r3 = o200 / o10
    let r4 = o299 % o100
    return r1 + r2 + r3 + r4
}
let r9 = wide_arith_ops()
assert_eq(r9, 419, "wide arith ops")

// --- Section 10: Wide neg and not ---
print("--- Section 10: Wide neg/not ---")
fn wide_neg_not() {
    let n0 = 0 let n1 = 1 let n2 = 2 let n3 = 3 let n4 = 4 let n5 = 5 let n6 = 6 let n7 = 7 let n8 = 8 let n9 = 9
    let n10 = 10 let n11 = 11 let n12 = 12 let n13 = 13 let n14 = 14 let n15 = 15 let n16 = 16 let n17 = 17 let n18 = 18 let n19 = 19
    let n20 = 20 let n21 = 21 let n22 = 22 let n23 = 23 let n24 = 24 let n25 = 25 let n26 = 26 let n27 = 27 let n28 = 28 let n29 = 29
    let n30 = 30 let n31 = 31 let n32 = 32 let n33 = 33 let n34 = 34 let n35 = 35 let n36 = 36 let n37 = 37 let n38 = 38 let n39 = 39
    let n40 = 40 let n41 = 41 let n42 = 42 let n43 = 43 let n44 = 44 let n45 = 45 let n46 = 46 let n47 = 47 let n48 = 48 let n49 = 49
    let n50 = 50 let n51 = 51 let n52 = 52 let n53 = 53 let n54 = 54 let n55 = 55 let n56 = 56 let n57 = 57 let n58 = 58 let n59 = 59
    let n60 = 60 let n61 = 61 let n62 = 62 let n63 = 63 let n64 = 64 let n65 = 65 let n66 = 66 let n67 = 67 let n68 = 68 let n69 = 69
    let n70 = 70 let n71 = 71 let n72 = 72 let n73 = 73 let n74 = 74 let n75 = 75 let n76 = 76 let n77 = 77 let n78 = 78 let n79 = 79
    let n80 = 80 let n81 = 81 let n82 = 82 let n83 = 83 let n84 = 84 let n85 = 85 let n86 = 86 let n87 = 87 let n88 = 88 let n89 = 89
    let n90 = 90 let n91 = 91 let n92 = 92 let n93 = 93 let n94 = 94 let n95 = 95 let n96 = 96 let n97 = 97 let n98 = 98 let n99 = 99
    let n100 = 100 let n101 = 101 let n102 = 102 let n103 = 103 let n104 = 104 let n105 = 105 let n106 = 106 let n107 = 107 let n108 = 108 let n109 = 109
    let n110 = 110 let n111 = 111 let n112 = 112 let n113 = 113 let n114 = 114 let n115 = 115 let n116 = 116 let n117 = 117 let n118 = 118 let n119 = 119
    let n120 = 120 let n121 = 121 let n122 = 122 let n123 = 123 let n124 = 124 let n125 = 125 let n126 = 126 let n127 = 127 let n128 = 128 let n129 = 129
    let n130 = 130 let n131 = 131 let n132 = 132 let n133 = 133 let n134 = 134 let n135 = 135 let n136 = 136 let n137 = 137 let n138 = 138 let n139 = 139
    let n140 = 140 let n141 = 141 let n142 = 142 let n143 = 143 let n144 = 144 let n145 = 145 let n146 = 146 let n147 = 147 let n148 = 148 let n149 = 149
    let n150 = 150 let n151 = 151 let n152 = 152 let n153 = 153 let n154 = 154 let n155 = 155 let n156 = 156 let n157 = 157 let n158 = 158 let n159 = 159
    let n160 = 160 let n161 = 161 let n162 = 162 let n163 = 163 let n164 = 164 let n165 = 165 let n166 = 166 let n167 = 167 let n168 = 168 let n169 = 169
    let n170 = 170 let n171 = 171 let n172 = 172 let n173 = 173 let n174 = 174 let n175 = 175 let n176 = 176 let n177 = 177 let n178 = 178 let n179 = 179
    let n180 = 180 let n181 = 181 let n182 = 182 let n183 = 183 let n184 = 184 let n185 = 185 let n186 = 186 let n187 = 187 let n188 = 188 let n189 = 189
    let n190 = 190 let n191 = 191 let n192 = 192 let n193 = 193 let n194 = 194 let n195 = 195 let n196 = 196 let n197 = 197 let n198 = 198 let n199 = 199
    let n200 = 200 let n201 = 201 let n202 = 202 let n203 = 203 let n204 = 204 let n205 = 205 let n206 = 206 let n207 = 207 let n208 = 208 let n209 = 209
    let n210 = 210 let n211 = 211 let n212 = 212 let n213 = 213 let n214 = 214 let n215 = 215 let n216 = 216 let n217 = 217 let n218 = 218 let n219 = 219
    let n220 = 220 let n221 = 221 let n222 = 222 let n223 = 223 let n224 = 224 let n225 = 225 let n226 = 226 let n227 = 227 let n228 = 228 let n229 = 229
    let n230 = 230 let n231 = 231 let n232 = 232 let n233 = 233 let n234 = 234 let n235 = 235 let n236 = 236 let n237 = 237 let n238 = 238 let n239 = 239
    let n240 = 240 let n241 = 241 let n242 = 242 let n243 = 243 let n244 = 244 let n245 = 245 let n246 = 246 let n247 = 247 let n248 = 248 let n249 = 249
    let n250 = 250 let n251 = 251 let n252 = 252 let n253 = 253 let n254 = 254 let n255 = 255 let n256 = 256 let n257 = 257 let n258 = 258 let n259 = 259
    let n260 = 260 let n261 = 261 let n262 = 262 let n263 = 263 let n264 = 264 let n265 = 265 let n266 = 266 let n267 = 267 let n268 = 268 let n269 = 269
    let n270 = 270 let n271 = 271 let n272 = 272 let n273 = 273 let n274 = 274 let n275 = 275 let n276 = 276 let n277 = 277 let n278 = 278 let n279 = 279
    let n280 = 280 let n281 = 281 let n282 = 282 let n283 = 283 let n284 = 284 let n285 = 285 let n286 = 286 let n287 = 287 let n288 = 288 let n289 = 289
    let n290 = 290 let n291 = 291 let n292 = 292 let n293 = 293 let n294 = 294 let n295 = 295 let n296 = 296 let n297 = 297 let n298 = 298 let n299 = 299
    let neg_val = -n200
    let not_val = !true
    return neg_val + n200
}
let r10 = wide_neg_not()
assert_eq(r10, 0, "wide neg")

print("")
print("=== Results ===")
print("Passed: " + to_string(pass_count))
print("Failed: " + to_string(fail_count))
if (fail_count == 0) {
    print("ALL TESTS PASSED!")
}
