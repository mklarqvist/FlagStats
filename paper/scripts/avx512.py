def word2byte_array(array):
    assert len(array) == 32
    res = []
    for word in array:
        assert word >= 0
        assert word <= 0xffff
        res.append(word & 0xff)
        res.append(word >> 8)

    return res


def avx512_dwords(array):
    assert len(array) == 64
    dwords = []
    for i in range(0, 64, 4):
        b0 = array[i + 0]
        b1 = array[i + 1]
        b2 = array[i + 2]
        b3 = array[i + 3]

        dword = (b3 << 24) | (b2 << 16) | (b1 << 8) | b0
        dwords.append(dword)

    return dwords


indent = ' ' * 4

def avx512_const(array):
    dwords = avx512_dwords(array)
    lo = ', '.join('0x%08x' % v for v in dwords[:8])
    hi = ', '.join('0x%08x' % v for v in dwords[8:])

    return f"_mm512_setr_epi32(\n{indent}{lo},\n{indent}{hi}\n);"


def avx512_var(name, array):
    dwords = avx512_dwords(array)
    lo = ', '.join('0x%08x' % v for v in dwords[:8])
    hi = ', '.join('0x%08x' % v for v in dwords[8:])

    return f"{indent}const __m512i {name} = _mm512_setr_epi32(\n" \
           f"{indent}{indent}{lo},\n" \
           f"{indent}{indent}{hi}\n" \
           f"{indent});"
