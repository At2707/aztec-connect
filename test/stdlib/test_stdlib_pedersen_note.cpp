#include <gtest/gtest.h>

#include <barretenberg/curves/grumpkin/grumpkin.hpp>
#include <barretenberg/waffle/composer/turbo_composer.hpp>
#include <barretenberg/waffle/proof_system/preprocess.hpp>
#include <barretenberg/waffle/proof_system/prover/prover.hpp>
#include <barretenberg/waffle/proof_system/verifier/verifier.hpp>
#include <barretenberg/waffle/proof_system/widgets/arithmetic_widget.hpp>

#include <barretenberg/waffle/stdlib/bitarray/bitarray.hpp>
#include <barretenberg/waffle/stdlib/common.hpp>
#include <barretenberg/waffle/stdlib/crypto/commitment/pedersen_note.hpp>
#include <barretenberg/waffle/stdlib/group/group_utils.hpp>
#include <iostream>
#include <memory>

using namespace barretenberg;
using namespace plonk;

typedef stdlib::field_t<waffle::TurboComposer> field_t;
typedef stdlib::witness_t<waffle::TurboComposer> witness_t;
typedef stdlib::witness_t<waffle::TurboComposer> public_witness_t;

TEST(stdlib_pedersen_note, test_new_pedersen_note)
{
    waffle::TurboComposer composer = waffle::TurboComposer();

    grumpkin::g1::affine_element note_owner_pub_key = grumpkin::g1::random_affine_element();

    fr::field_t view_key_value = fr::random_element();
    view_key_value.data[3] = view_key_value.data[3] & 0x3FFFFFFFFFFFFFFFULL;
    view_key_value = fr::to_montgomery_form(view_key_value);

    fr::field_t note_value = fr::to_montgomery_form({ { 9999, 0, 0, 0 } });

    grumpkin::g1::element left = plonk::stdlib::group_utils::fixed_base_scalar_mul<32>(note_value, 0);
    grumpkin::g1::element right = plonk::stdlib::group_utils::fixed_base_scalar_mul<250>(view_key_value, 1);
    grumpkin::g1::element expected;
    grumpkin::g1::add(left, right, expected);

    grumpkin::g1::affine_element hashed_pub_key =
        plonk::stdlib::group_utils::compress_to_point_native(note_owner_pub_key.x, note_owner_pub_key.y);

    grumpkin::g1::mixed_add(expected, hashed_pub_key, expected);
    expected = grumpkin::g1::normalize(expected);

    field_t view_key = witness_t(&composer, view_key_value);
    field_t note_value_field = witness_t(&composer, note_value);
    field_t note_owner_x = witness_t(&composer, note_owner_pub_key.x);
    field_t note_owner_y = witness_t(&composer, note_owner_pub_key.y);

    field_t ciphertext_x = public_witness_t(&composer, expected.x);
    field_t ciphertext_y = public_witness_t(&composer, expected.y);
    plonk::stdlib::pedersen_note::public_note target_encryption{ { ciphertext_x, ciphertext_y } };

    plonk::stdlib::uint<waffle::TurboComposer> value(32, note_value_field);
    plonk::stdlib::pedersen_note::private_note plaintext{ { note_owner_x, note_owner_y }, value, view_key };

    plonk::stdlib::pedersen_note::public_note result = plonk::stdlib::pedersen_note::encrypt_note(plaintext);
    composer.assert_equal(result.ciphertext.x.witness_index, target_encryption.ciphertext.x.witness_index);
    composer.assert_equal(result.ciphertext.y.witness_index, target_encryption.ciphertext.y.witness_index);

    waffle::TurboProver prover = composer.preprocess();

    printf("composer gates = %zu\n", composer.get_num_gates());
    waffle::TurboVerifier verifier = composer.create_verifier();

    waffle::plonk_proof proof = prover.construct_proof();

    bool proof_result = verifier.verify_proof(proof);
    EXPECT_EQ(proof_result, true);
}

TEST(stdlib_pedersen_note, test_new_pedersen_note_zero)
{
    waffle::TurboComposer composer = waffle::TurboComposer();

    grumpkin::g1::affine_element note_owner_pub_key = grumpkin::g1::random_affine_element();

    fr::field_t view_key_value = fr::random_element();
    view_key_value.data[3] = view_key_value.data[3] & 0x3FFFFFFFFFFFFFFFULL;
    view_key_value = fr::to_montgomery_form(view_key_value);

    fr::field_t note_value = fr::to_montgomery_form({ { 0, 0, 0, 0 } });

    grumpkin::g1::element expected = plonk::stdlib::group_utils::fixed_base_scalar_mul<32>(note_value, 0);

    grumpkin::g1::affine_element hashed_pub_key =
        plonk::stdlib::group_utils::compress_to_point_native(note_owner_pub_key.x, note_owner_pub_key.y);

    grumpkin::g1::mixed_add(expected, hashed_pub_key, expected);
    expected = grumpkin::g1::normalize(expected);

    field_t view_key = witness_t(&composer, view_key_value);
    field_t note_value_field = witness_t(&composer, note_value);
    field_t note_owner_x = witness_t(&composer, note_owner_pub_key.x);
    field_t note_owner_y = witness_t(&composer, note_owner_pub_key.y);

    field_t ciphertext_x = public_witness_t(&composer, expected.x);
    field_t ciphertext_y = public_witness_t(&composer, expected.y);

    plonk::stdlib::pedersen_note::public_note target_encryption{ { ciphertext_x, ciphertext_y } };
    plonk::stdlib::uint<waffle::TurboComposer> value(32, note_value_field);

    plonk::stdlib::pedersen_note::private_note plaintext{ { note_owner_x, note_owner_y }, value, view_key };

    plonk::stdlib::pedersen_note::public_note result = plonk::stdlib::pedersen_note::encrypt_note(plaintext);
    composer.assert_equal(result.ciphertext.x.witness_index, target_encryption.ciphertext.x.witness_index);
    composer.assert_equal(result.ciphertext.y.witness_index, target_encryption.ciphertext.y.witness_index);

    waffle::TurboProver prover = composer.preprocess();

    printf("composer gates = %zu\n", composer.get_num_gates());
    waffle::TurboVerifier verifier = composer.create_verifier();

    waffle::plonk_proof proof = prover.construct_proof();

    bool proof_result = verifier.verify_proof(proof);
    EXPECT_EQ(proof_result, true);
}
