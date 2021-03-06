#include "traditional_gkr/prover_bc_clogc.h"
#include <cstring>
#include <cassert>

void prover::get_circuit(const blocked_circuit &from_verifier)
{
	C = from_verifier;
}

prime_field::field_element prover::V_res(const prime_field::field_element* one_minus_r_0, const prime_field::field_element* r_0, 
	const prime_field::field_element* one_minus_r_b, const prime_field::field_element* r_b, const prime_field::field_element* output_raw, int r_0_size, int r_b_size, int output_size)
{
	std::chrono::high_resolution_clock::time_point t0 = std::chrono::high_resolution_clock::now();
	prime_field::field_element *output;
	output = new prime_field::field_element[output_size];
	for(int i = 0; i < output_size; ++i)
		output[i] = output_raw[i];
	
	for(int i = 0; i < r_0_size; ++i)
	{
		for(int j = 0; j < (output_size >> 1); ++j)
		{
			output[j].value = (output[j << 1].value * one_minus_r_0[i].value + output[j << 1 | 1].value * r_0[i].value) % prime_field::mod;
		}
		output_size >>= 1;
	}
	for(int i = 0; i < r_b_size; ++i)
	{
		for(int j = 0; j < (output_size >> 1); ++j)
		{
			output[j].value = (output[j << 1].value * one_minus_r_b[i].value + output[j << 1 | 1].value * r_b[i].value) % prime_field::mod;
		}
		output_size >>= 1;
	}
	std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();

	std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t1 - t0);
	total_time += time_span.count();
	prime_field::field_element res = output[0];
	delete[] output;

	if(res.value < 0)
		res.value = res.value + prime_field::mod;
	return res;
}

prime_field::field_element* prover::evaluate()
{
	std::chrono::high_resolution_clock::time_point t0 = std::chrono::high_resolution_clock::now();
	circuit_value = new prime_field::field_element**[C.total_blocks];
	for(int i = 0; i < C.total_blocks; ++i)
		circuit_value[i] = new prime_field::field_element*[C.blocks[0].total_depth];
	for(int blk = 0; blk < C.total_blocks; ++blk)
	{
		circuit_value[blk][0] = new prime_field::field_element[(1 << C.blocks[blk].circuit[0].bit_length)];
		for(int i = 0; i < (1 << C.blocks[blk].circuit[0].bit_length); ++i)
		{
			int g, u, v, ty;
			g = i;
			u = C.blocks[blk].circuit[0].gates[g].u;
			v = C.blocks[blk].circuit[0].gates[g].v;
			ty = C.blocks[blk].circuit[0].gates[g].ty;
			assert(ty == 3 || ty == 2);
			circuit_value[blk][0][g] = prime_field::field_element(u);
		}
		for(int i = 1; i < C.blocks[blk].total_depth; ++i)
		{
			circuit_value[blk][i] = new prime_field::field_element[(1 << C.blocks[blk].circuit[i].bit_length)];
			for(int j = 0; j < (1 << C.blocks[blk].circuit[i].bit_length); ++j)
			{
				int g, u, v, ty;
				g = j;
				ty = C.blocks[blk].circuit[i].gates[g].ty;
				u = C.blocks[blk].circuit[i].gates[g].u;
				v = C.blocks[blk].circuit[i].gates[g].v;
				if(ty == 0)
				{
					circuit_value[blk][i][g] = circuit_value[blk][i - 1][u] + circuit_value[blk][i - 1][v];
				}
				else if(ty == 1)
				{
					circuit_value[blk][i][g] = circuit_value[blk][i - 1][u] * circuit_value[blk][i - 1][v];
				}
				else if(ty == 2)
				{
					circuit_value[blk][i][g] = prime_field::field_element(0);
				}
				else if(ty == 3)
				{
					circuit_value[blk][i][g] = prime_field::field_element(u);
				}
				else
				{
					assert(false);
				}
			}
		}
	}
/*
	//cheating prover test
	ret[0].second =  prime_field::field_element(111);
*/
	std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();

	std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t1 - t0);
	std::cerr << "total evaluation time: " << time_span.count() << " seconds." << std::endl;
	prime_field::field_element *result;
	result = new prime_field::field_element[C.total_blocks * (1 << C.blocks[0].circuit[C.blocks[0].total_depth - 1].bit_length)];
	int bs = (1 << C.blocks[0].circuit[C.blocks[0].total_depth - 1].bit_length);
	for(int i = 0; i < C.total_blocks; ++i)
		for(int j = 0; j < bs; ++j)
			result[i * bs + j] = circuit_value[i][C.blocks[0].total_depth - 1][j];
	return result;
}

void prover::sumcheck_init(int layer_id, int block_bn, int bit_length_g, int bit_length_u, int bit_length_v, 
	const prime_field::field_element &a, const prime_field::field_element &b, 
	const prime_field::field_element* R_B_0,
	const prime_field::field_element* R_0, const prime_field::field_element* R_1,
	prime_field::field_element* o_r_b_0,
	prime_field::field_element* o_r_0, prime_field::field_element *o_r_1)
{
	r_0 = R_0;
	r_1 = R_1;
	r_b_0 = R_B_0;
	alpha = a;
	beta = b;
	sumcheck_layer_id = layer_id;
	length_g = bit_length_g;
	length_u = bit_length_u;
	length_v = bit_length_v;
	one_minus_r_0 = o_r_0;
	one_minus_r_1 = o_r_1;
	one_minus_r_b_0 = o_r_b_0;
	block_binary_length = block_bn;
}

void prover::init_array(int max_bit_length, int blk_bit_length)
{
	add_linear = new quadratic_poly[(1 << max_bit_length)];
//	multV_linear = new linear_poly[(1 << max_bit_length)];
	mult_linear = new quadratic_poly[(1 << max_bit_length)];
//	addV_linear = new linear_poly[(1 << max_bit_length)];
	V_mult_add = new prime_field::field_element[(1 << max_bit_length)];
	V_mult_add_copy = new prime_field::field_element[(1 << max_bit_length)];
	beta_g_sum = new prime_field::field_element[(1 << max_bit_length)];
	beta_u = new prime_field::field_element[(1 << max_bit_length)];
	int half_length = (max_bit_length >> 1) + 1;
	beta_g_r0_fhalf = new prime_field::field_element[(1 << half_length)];
	beta_g_r0_shalf = new prime_field::field_element[(1 << half_length)];
	beta_g_r1_fhalf = new prime_field::field_element[(1 << half_length)];
	beta_g_r1_shalf = new prime_field::field_element[(1 << half_length)];
	beta_u_fhalf = new prime_field::field_element[(1 << half_length)];
	beta_u_shalf = new prime_field::field_element[(1 << half_length)];
	block_beta_value = new prime_field::field_element[(1 << blk_bit_length)];
	block_v_value = new prime_field::field_element[1 << (blk_bit_length + max_bit_length)];
}

void prover::delete_self()
{
	delete[] add_linear;
	delete[] mult_linear;
	delete[] V_mult_add;
	delete[] beta_u;
	delete[] beta_g_sum;

	delete[] V_mult_add_copy;
	delete[] beta_g_r0_fhalf;
	delete[] beta_g_r0_shalf;
	delete[] beta_g_r1_fhalf;
	delete[] beta_g_r1_shalf;
	delete[] beta_u_fhalf;
	delete[] beta_u_shalf;
	for(int blk = 0; blk < (1 << C.total_blocks_binary_length); ++blk)
	{
		for(int i = 0; i < C.blocks[blk].total_depth; ++i)
			delete[] circuit_value[blk][i];
		delete[] circuit_value[blk];
	}
	delete[] circuit_value;
	delete[] block_beta_value;
	delete[] block_v_value;
}

prover::~prover()
{
}

void prover::sumcheck_phase0_init()
{
	std::chrono::high_resolution_clock::time_point t0 = std::chrono::high_resolution_clock::now();
	
	int total_blk = 1 << block_binary_length;
	block_beta_value[0] = prime_field::field_element(1);
	for(int i = 0; i < C.total_blocks_binary_length; ++i)
	{
		for(int j = 0; j < (1 << i); ++j)
		{
			block_beta_value[j | (1 << i)] = block_beta_value[j] * r_b_0[i];
			block_beta_value[j] = block_beta_value[j] * one_minus_r_b_0[i];
		}
	}

	beta_g_r0_fhalf[0] = alpha;
	beta_g_r1_fhalf[0] = beta;
	beta_g_r0_shalf[0] = prime_field::field_element(1);
	beta_g_r1_shalf[0] = prime_field::field_element(1);

	int first_half = length_g >> 1, second_half = length_g - first_half;

	for(int i = 0; i < first_half; ++i)
	{
		for(int j = 0; j < (1 << i); ++j)
		{
			beta_g_r0_fhalf[j | (1 << i)].value = beta_g_r0_fhalf[j].value * r_0[i].value % prime_field::mod;
			beta_g_r0_fhalf[j].value = beta_g_r0_fhalf[j].value * one_minus_r_0[i].value % prime_field::mod;
			beta_g_r1_fhalf[j | (1 << i)].value = beta_g_r1_fhalf[j].value * r_1[i].value % prime_field::mod;
			beta_g_r1_fhalf[j].value = beta_g_r1_fhalf[j].value * one_minus_r_1[i].value % prime_field::mod;
		}
	}

	for(int i = 0; i < second_half; ++i)
	{
		for(int j = 0; j < (1 << i); ++j)
		{
			beta_g_r0_shalf[j | (1 << i)].value = beta_g_r0_shalf[j].value * r_0[i + first_half].value % prime_field::mod;
			beta_g_r0_shalf[j].value = beta_g_r0_shalf[j].value * one_minus_r_0[i + first_half].value % prime_field::mod;
			beta_g_r1_shalf[j | (1 << i)].value = beta_g_r1_shalf[j].value * r_1[i + first_half].value % prime_field::mod;
			beta_g_r1_shalf[j].value = beta_g_r1_shalf[j].value * one_minus_r_1[i + first_half].value % prime_field::mod;
		}
	}

	int mask_fhalf = (1 << first_half) - 1;

	for(int i = 0; i < (1 << length_g); ++i)
	{
		beta_g_sum[i].value = (beta_g_r0_fhalf[i & mask_fhalf].value * beta_g_r0_shalf[i >> first_half].value 
							 + beta_g_r1_fhalf[i & mask_fhalf].value * beta_g_r1_shalf[i >> first_half].value) % prime_field::mod;
	}

	for(int i = 0; i < total_blk; ++i)
	{
		for(int j = 0; j < (1 << length_u); ++j)
		{
			block_v_value[i << length_u | j] = circuit_value[i][sumcheck_layer_id - 1][j];
		}
	}
	
	std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t1 - t0);
	double tmp = time_span.count();
	total_time += time_span.count();
}

cubic_poly prover::sumcheck_phase0_update(prime_field::field_element previous_random, int current_bit)
{
	std::chrono::high_resolution_clock::time_point t0 = std::chrono::high_resolution_clock::now();
	cubic_poly ret;
	ret = cubic_poly(prime_field::field_element(0), prime_field::field_element(0), prime_field::field_element(0), prime_field::field_element(0));
	
	if(current_bit != 0)
	{
		//update arrays
		for(int blk = 0; blk < (1 << (block_binary_length - current_bit)); ++blk)
		{
			prime_field::field_element zero_beta_point = block_beta_value[blk << 1 | 0];
			prime_field::field_element one_beta_point = block_beta_value[blk << 1 | 1];
			block_beta_value[blk] = (one_beta_point - zero_beta_point) * previous_random + zero_beta_point;
			for(int u = 0; u < (1 << length_u); ++u)
			{
				int blk_zero = blk << 1 | 0, blk_one = blk << 1 | 1;
				prime_field::field_element zero_point = block_v_value[blk_zero << length_u | u];
				prime_field::field_element one_point = block_v_value[blk_one << length_u | u];
				block_v_value[blk << length_u | u] = (one_point - zero_point) * previous_random + zero_point;
			}
		}
	}

	for(int blk = 0; blk < (1 << (block_binary_length - current_bit - 1)); ++blk)
	{
		prime_field::field_element zero_beta_point = block_beta_value[blk << 1 | 0];
		prime_field::field_element one_beta_point = block_beta_value[blk << 1 | 1];
		quadratic_poly v_part;
		linear_poly beta_part;
		beta_part = linear_poly(one_beta_point - zero_beta_point, zero_beta_point);
		v_part = quadratic_poly(prime_field::field_element(0), prime_field::field_element(0), prime_field::field_element(0));

		for(int g = 0; g < (1 << length_g); ++g)
		{
			int u = C.blocks[blk].circuit[sumcheck_layer_id].gates[g].u;
			int v = C.blocks[blk].circuit[sumcheck_layer_id].gates[g].v;
			int ty = C.blocks[blk].circuit[sumcheck_layer_id].gates[g].ty;

			linear_poly var_u_poly, var_v_poly;
			prime_field::field_element one_value, zero_value;
			
			int blk_one = blk << 1 | 1, blk_zero = blk << 1;
			
			one_value = block_v_value[blk_one << length_u | u];
			zero_value = block_v_value[blk_zero << length_u | u];
			var_u_poly = linear_poly(one_value - zero_value, zero_value);

			one_value = block_v_value[blk_one << length_u | v];
			zero_value = block_v_value[blk_zero << length_u | v];
			var_v_poly = linear_poly(one_value - zero_value, zero_value);

			switch(ty)
			{
				case 0:
					//add gate
					{
						linear_poly tmp = var_u_poly + var_v_poly;
						tmp.a = tmp.a * beta_g_sum[g];
						tmp.b = tmp.b * beta_g_sum[g];
						v_part = v_part + quadratic_poly(prime_field::field_element(0), tmp.a, tmp.b);
					}
					break;
				case 1:
					{
						quadratic_poly tmp = var_u_poly * var_v_poly;
						tmp.a = tmp.a * beta_g_sum[g];
						tmp.b = tmp.b * beta_g_sum[g];
						tmp.c = tmp.c * beta_g_sum[g];
						v_part = v_part + tmp;
					}
					break;
				case 2:
					//assert(false);
					break;
				case 3:
					break;
			}
		}

		ret = ret + v_part * beta_part;
	}
	std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t1 - t0);
	double tmp = time_span.count();
	total_time += time_span.count();
	return ret;
}

void prover::sumcheck_phase1_init(prime_field::field_element previous_random)
{
	std::chrono::high_resolution_clock::time_point t0 = std::chrono::high_resolution_clock::now();
	//mult init
	//O(n) cache friendly implementation of initialize beta values
	//CMT protocol uses an O(n log n) one
	total_uv = (1 << length_u);
	prime_field::field_element zero = prime_field::field_element(0);
	for(int i = 0; i < total_uv; ++i)
	{
		prime_field::field_element one_value, zero_value;
		one_value = block_v_value[1 << length_u | i];
		zero_value = block_v_value[i];

		V_mult_add[i] = (one_value - zero_value) * previous_random + zero_value;
		V_mult_add_copy[i] = V_mult_add[i];
	}
	beta_blk_val = (block_beta_value[1] - block_beta_value[0]) * previous_random + block_beta_value[0];
	
	std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t1 - t0);
	double tmp = time_span.count();
	total_time += time_span.count();
}

prime_field::field_element from_string(const char* str)
{
	prime_field::field_element ret = prime_field::field_element(0);
	int len = strlen(str);
	for(int i = 0; i < len; ++i)
	{
		int digit = str[i] - '0';
		ret = ret * prime_field::field_element(10) + prime_field::field_element(digit);
	}
	return ret;
}

quadratic_poly prover::sumcheck_phase1_update(prime_field::field_element previous_random, int current_bit)
{
	std::chrono::high_resolution_clock::time_point t0 = std::chrono::high_resolution_clock::now();
	quadratic_poly ret = quadratic_poly(prime_field::field_element(0), prime_field::field_element(0), prime_field::field_element(0));
	
	int total_g = (1 << length_g);
	//re-implement CMT protocol

	//update V using latest randomness
	if(current_bit - 1 >= 0)
	{
		int previous_bit = current_bit - 1;
		for(int i = 0; i < (total_uv >> current_bit); ++i)
		{
			V_mult_add[i] = V_mult_add[i << 1] * (prime_field::field_element(1) - previous_random)
						  + V_mult_add[i << 1 | 1] * previous_random;
		}
		for(int i = 0; i < total_g; ++i)
		{
			int u = C.blocks[0].circuit[sumcheck_layer_id].gates[i].u;
			if(((u >> previous_bit) & 1) == 0)
			{
				beta_g_sum[i] = beta_g_sum[i] * (prime_field::field_element(1) - previous_random);
			}
			else
			{
				beta_g_sum[i] = beta_g_sum[i] * previous_random;
			}
		}
	}
	for(int i = 0; i < (total_uv >> (current_bit + 1)); ++i)
	{
		add_linear[i] = quadratic_poly(prime_field::field_element(0), prime_field::field_element(0), prime_field::field_element(0));
		mult_linear[i] = quadratic_poly(prime_field::field_element(0), prime_field::field_element(0), prime_field::field_element(0));
	}
	quadratic_poly contribution;
	for(int i = 0; i < total_g; ++i)
	{
		int ty = C.blocks[0].circuit[sumcheck_layer_id].gates[i].ty;
		int u = C.blocks[0].circuit[sumcheck_layer_id].gates[i].u;
		int v = C.blocks[0].circuit[sumcheck_layer_id].gates[i].v;
		int target_linear = u >> (current_bit + 1);
		int current_u_bit = (u >> current_bit) & 1;
		switch(ty)
		{
			case 0: //add gate
			{
				if(current_u_bit == 0)
				{
					add_linear[target_linear].c = add_linear[target_linear].c + beta_g_sum[i] * (V_mult_add[u >> current_bit] + V_mult_add_copy[v]);
					add_linear[target_linear].a = add_linear[target_linear].a + (prime_field::field_element(0) - beta_g_sum[i]) * (V_mult_add_copy[v] - V_mult_add[u >> current_bit] + prime_field::field_element(2) * V_mult_add[(u >> current_bit) ^ 1]);
				}
				else
				{
					add_linear[target_linear].b = add_linear[target_linear].b + beta_g_sum[i] * (V_mult_add[u >> current_bit] + V_mult_add_copy[v]);
					add_linear[target_linear].a = add_linear[target_linear].a + (beta_g_sum[i] + beta_g_sum[i]) * (V_mult_add_copy[v] + V_mult_add[u >> current_bit] + V_mult_add[u >> current_bit] - V_mult_add[(u >> current_bit) ^ 1]);
				}
				break;
			}
			case 1: //mult gate
			{
				if(current_u_bit == 0)
				{
					mult_linear[target_linear].c = mult_linear[target_linear].c + beta_g_sum[i] * (V_mult_add[u >> current_bit] * V_mult_add_copy[v]);
					mult_linear[target_linear].a = mult_linear[target_linear].a + (prime_field::field_element(0) - beta_g_sum[i]) * V_mult_add_copy[v] * (V_mult_add[(u >> current_bit) ^ 1] + V_mult_add[(u >> current_bit) ^ 1] - V_mult_add[u >> current_bit]);
				}
				else
				{
					mult_linear[target_linear].b = mult_linear[target_linear].b + beta_g_sum[i] * V_mult_add[u >> current_bit] * V_mult_add_copy[v];
					mult_linear[target_linear].a = mult_linear[target_linear].a + (beta_g_sum[i] + beta_g_sum[i]) * V_mult_add_copy[v] * (V_mult_add[u >> current_bit] + V_mult_add[u >> current_bit] - V_mult_add[(u >> current_bit) ^ 1]);
				}
				break;
			}
			case 2: //dummy gate skip
				break;
			case 3: //input gate, should not be executed
				assert(false);
				break;
			default:
				assert(false);
				break;
		}
	}

	//inverse of 2
	prime_field::field_element two = prime_field::field_element(2);
	prime_field::field_element inv_2 = from_string("8399054365507916142470402071115866954879789801702376374514189432082785107975");
	for(int i = 0; i < (total_uv >> (current_bit + 1)); ++i)
	{
		prime_field::field_element ta, tb, tc;

		tc = add_linear[i].c, tb = add_linear[i].b, ta = add_linear[i].a;
		add_linear[i] = quadratic_poly((ta - tc) * inv_2 - (tb - tc), two * (tb - tc) - (ta - tc) * inv_2, tc);

		tc = mult_linear[i].c, tb = mult_linear[i].b, ta = mult_linear[i].a;
		mult_linear[i] = quadratic_poly((ta - tc) * inv_2 - (tb - tc), two * (tb - tc) - (ta - tc) * inv_2, tc);
		ret = ret + add_linear[i] + mult_linear[i];
	}

	ret.a = ret.a * beta_blk_val;
	ret.b = ret.b * beta_blk_val;
	ret.c = ret.c * beta_blk_val;

	std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();

	std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t1 - t0);
	total_time += time_span.count();
	return ret;
}

void prover::sumcheck_phase2_init(prime_field::field_element previous_random)
{
	std::chrono::high_resolution_clock::time_point t0 = std::chrono::high_resolution_clock::now();
	
	v_u = V_mult_add[0] * (prime_field::field_element(1) - previous_random) + V_mult_add[1] * previous_random;
	for(int i = 0; i < total_uv; ++i)
	{
		V_mult_add[i] = V_mult_add_copy[i];
	}
	int previous_bit = C.blocks[0].circuit[sumcheck_layer_id - 1].bit_length - 1;
	int total_g = (1 << length_g);
	for(int i = 0; i < total_g; ++i)
	{
		int u = C.blocks[0].circuit[sumcheck_layer_id].gates[i].u;
		if(((u >> previous_bit) & 1) == 0)
		{
			beta_g_sum[i] = beta_g_sum[i] * (prime_field::field_element(1) - previous_random);
		}
		else
		{
			beta_g_sum[i] = beta_g_sum[i] * previous_random;
		}
	}
	std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();

	std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t1 - t0);
	total_time += time_span.count();
}

quadratic_poly prover::sumcheck_phase2_update(prime_field::field_element previous_random, int current_bit)
{
	std::chrono::high_resolution_clock::time_point t0 = std::chrono::high_resolution_clock::now();
	quadratic_poly ret = quadratic_poly(prime_field::field_element(0), prime_field::field_element(0), prime_field::field_element(0));

	int total_g = (1 << length_g);
	//re-implement CMT protocol

	//update V using latest randomness
	if(current_bit - 1 >= 0)
	{
		int previous_bit = current_bit - 1;
		for(int i = 0; i < (total_uv >> current_bit); ++i)
		{
			V_mult_add[i] = V_mult_add[i << 1] * (prime_field::field_element(1) - previous_random)
						  + V_mult_add[i << 1 | 1] * previous_random;
		}
		for(int i = 0; i < total_g; ++i)
		{
			int v = C.blocks[0].circuit[sumcheck_layer_id].gates[i].v;
			if(((v >> previous_bit) & 1) == 0)
			{
				beta_g_sum[i] = beta_g_sum[i] * (prime_field::field_element(1) - previous_random);
			}
			else
			{
				beta_g_sum[i] = beta_g_sum[i] * previous_random;
			}
		}
	}
	for(int i = 0; i < (total_uv >> (current_bit + 1)); ++i)
	{
		add_linear[i] = quadratic_poly(prime_field::field_element(0), prime_field::field_element(0), prime_field::field_element(0));
		mult_linear[i] = quadratic_poly(prime_field::field_element(0), prime_field::field_element(0), prime_field::field_element(0));
	}
	quadratic_poly contribution;
	for(int i = 0; i < total_g; ++i)
	{
		int ty = C.blocks[0].circuit[sumcheck_layer_id].gates[i].ty;
		int v = C.blocks[0].circuit[sumcheck_layer_id].gates[i].v;
		int target_linear = v >> (current_bit + 1);
		int current_v_bit = (v >> current_bit) & 1;
		switch(ty)
		{
			case 0: //add gate
			{
				if(current_v_bit == 0)
				{
					add_linear[target_linear].c = add_linear[target_linear].c + beta_g_sum[i] * (V_mult_add[v >> current_bit] + v_u);
					add_linear[target_linear].a = add_linear[target_linear].a + (prime_field::field_element(0) - beta_g_sum[i]) * (v_u - V_mult_add[v >> current_bit] + prime_field::field_element(2) * V_mult_add[(v >> current_bit) ^ 1]);
				}
				else
				{
					add_linear[target_linear].b = add_linear[target_linear].b + beta_g_sum[i] * (V_mult_add[v >> current_bit] + v_u);
					add_linear[target_linear].a = add_linear[target_linear].a + (beta_g_sum[i] + beta_g_sum[i]) * (v_u + V_mult_add[v >> current_bit] + V_mult_add[v >> current_bit] - V_mult_add[(v >> current_bit) ^ 1]);
				}
				break;
			}
			case 1: //mult gate
			{
				if(current_v_bit == 0)
				{
					mult_linear[target_linear].c = mult_linear[target_linear].c + beta_g_sum[i] * (V_mult_add[v >> current_bit] * v_u);
					mult_linear[target_linear].a = mult_linear[target_linear].a + (prime_field::field_element(0) - beta_g_sum[i]) * v_u * (V_mult_add[(v >> current_bit) ^ 1] + V_mult_add[(v >> current_bit) ^ 1] - V_mult_add[v >> current_bit]);
				}
				else
				{
					mult_linear[target_linear].b = mult_linear[target_linear].b + beta_g_sum[i] * V_mult_add[v >> current_bit] * v_u;
					mult_linear[target_linear].a = mult_linear[target_linear].a + (beta_g_sum[i] + beta_g_sum[i]) * v_u * (V_mult_add[v >> current_bit] + V_mult_add[v >> current_bit] - V_mult_add[(v >> current_bit) ^ 1]);
				}
				break;
			}
			case 2: //dummy gate skip
				break;
			case 3: //input gate, should not be executed
				assert(false);
				break;
			default:
				assert(false);
				break;
		}
	}

	//inverse of 2
	prime_field::field_element two = prime_field::field_element(2);
	prime_field::field_element inv_2 = from_string("8399054365507916142470402071115866954879789801702376374514189432082785107975");
	for(int i = 0; i < (total_uv >> (current_bit + 1)); ++i)
	{
		prime_field::field_element ta, tb, tc;

		tc = add_linear[i].c, tb = add_linear[i].b, ta = add_linear[i].a;
		add_linear[i] = quadratic_poly((ta - tc) * inv_2 - (tb - tc), two * (tb - tc) - (ta - tc) * inv_2, tc);

		tc = mult_linear[i].c, tb = mult_linear[i].b, ta = mult_linear[i].a;
		mult_linear[i] = quadratic_poly((ta - tc) * inv_2 - (tb - tc), two * (tb - tc) - (ta - tc) * inv_2, tc);
		ret = ret + add_linear[i] + mult_linear[i];
	}

	ret.a = ret.a * beta_blk_val;
	ret.b = ret.b * beta_blk_val;
	ret.c = ret.c * beta_blk_val;

	std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t1 - t0);
	total_time += time_span.count();
	return ret;
}
std::pair<prime_field::field_element, prime_field::field_element> prover::sumcheck_finalize(prime_field::field_element previous_random)
{
	v_v = V_mult_add[0] * (prime_field::field_element(1) - previous_random) + V_mult_add[1] * previous_random;
	return std::make_pair(v_u, v_v);
}
void prover::proof_init()
{
	//todo
}
