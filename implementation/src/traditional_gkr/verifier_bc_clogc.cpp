#include "traditional_gkr/verifier_bc_clogc.h"
#include <string>
#include <utility>
#include <iostream>
#include "linear_gkr/random_generator.h"

void verifier::get_prover(prover *pp)
{
	p = pp;
}
void verifier::set_blocks(int num_blocks, int binary_bn)
{
	C.blocks = new layered_circuit[num_blocks];
	C.total_blocks = num_blocks;
	C.total_blocks_binary_length = binary_bn;
}
void verifier::read_circuit_from_string(char* file, int block_id)
{
	int d;
	static char str[300];
	int offset;
	sscanf(file, "%d%n", &d, &offset);
	file += offset;
	int n;
	C.blocks[block_id].circuit = new layer[d];
	C.blocks[block_id].total_depth = d;
	int max_bit_length = -1;
	for(int i = 0; i < d; ++i)
	{
		sscanf(file, "%d%n", &n, &offset);
		file += offset;
		if(n == 1)
			C.blocks[block_id].circuit[i].gates = new gate[2];
		else
			C.blocks[block_id].circuit[i].gates = new gate[n];
		int max_gate = -1;
		int previous_g = -1;
		for(int j = 0; j < n; ++j)
		{
			int ty, g, u, v;
			sscanf(file, "%d%d%d%d%n", &ty, &g, &u, &v, &offset);
			file += offset;
			if(g != previous_g + 1)
			{
				printf("Error, gates must be in sorted order, and full [0, 2^n - 1].");
			}
			previous_g = g;
			C.blocks[block_id].circuit[i].gates[g] = gate(ty, u, v);
		}
		max_gate = previous_g;
		int cnt = 0;
		while(max_gate)
		{
			cnt++;
			max_gate >>= 1;
		}
		max_gate = 1;
		while(cnt)
		{
			max_gate <<= 1;
			cnt--;
		}
		int mx_gate = max_gate;
		while(mx_gate)
		{
			cnt++;
			mx_gate >>= 1;
		}
		if(n == 1)
		{
			//add a dummy gate to avoid ill-defined layer.
			C.blocks[block_id].circuit[i].gates[max_gate] = gate(2, 0, 0);
			C.blocks[block_id].circuit[i].bit_length = cnt;
		}
		else
		{
			C.blocks[block_id].circuit[i].bit_length = cnt - 1;
		}
		fprintf(stderr, "layer %d, bit_length %d\n", i, C.blocks[block_id].circuit[i].bit_length);
		if(C.blocks[block_id].circuit[i].bit_length > max_bit_length)
			max_bit_length = C.blocks[block_id].circuit[i].bit_length;
	}
	p -> init_array(max_bit_length, C.total_blocks_binary_length);

	beta_g_r0 = new prime_field::field_element[(1 << max_bit_length)];
	beta_g_r1 = new prime_field::field_element[(1 << max_bit_length)];
	beta_v = new prime_field::field_element[(1 << max_bit_length)];
	beta_u = new prime_field::field_element[(1 << max_bit_length)];
}

prime_field::field_element verifier::add(int depth)
{
	//brute force for sanity check
	prime_field::field_element ret = prime_field::field_element(0);
	for(int i = 0; i < (1 << C.blocks[0].circuit[depth].bit_length); ++i)
	{
		int g = i, u = C.blocks[0].circuit[depth].gates[i].u, v = C.blocks[0].circuit[depth].gates[i].v;
		if(C.blocks[0].circuit[depth].gates[i].ty == 0)
		{
			ret = ret + (beta_g_r0[g] + beta_g_r1[g]) * beta_u[u] * beta_v[v];
		}
	}
	ret.value = ret.value % prime_field::mod;
	if(ret.value < 0)
		ret.value = ret.value + prime_field::mod;
	return ret;
}
prime_field::field_element verifier::mult(int depth)
{
	prime_field::field_element ret = prime_field::field_element(0);
	for(int i = 0; i < (1 << C.blocks[0].circuit[depth].bit_length); ++i)
	{
		int g = i, u = C.blocks[0].circuit[depth].gates[i].u, v = C.blocks[0].circuit[depth].gates[i].v;
		if(C.blocks[0].circuit[depth].gates[i].ty == 1)
		{
			ret = ret + (beta_g_r0[g] + beta_g_r1[g]) * beta_u[u] * beta_v[v];
		}
	}
	ret.value = ret.value % prime_field::mod;
	if(ret.value < 0)
		ret.value = ret.value + prime_field::mod;
	return ret;
}

void verifier::beta_init(int depth, prime_field::field_element alpha, prime_field::field_element beta,
	const prime_field::field_element* r_0, const prime_field::field_element* r_1, 
	const prime_field::field_element* r_u, const prime_field::field_element* r_v, 
	const prime_field::field_element* one_minus_r_0, const prime_field::field_element* one_minus_r_1, 
	const prime_field::field_element* one_minus_r_u, const prime_field::field_element* one_minus_r_v)
{
	beta_g_r0[0] = alpha;
	beta_g_r1[0] = beta;
	for(int i = 0; i < C.blocks[0].circuit[depth].bit_length; ++i)
	{
		for(int j = 0; j < (1 << i); ++j)
		{
			beta_g_r0[j | (1 << i)].value = beta_g_r0[j].value * r_0[i].value % prime_field::mod;
			beta_g_r1[j | (1 << i)].value = beta_g_r1[j].value * r_1[i].value % prime_field::mod;
		}
		for(int j = 0; j < (1 << i); ++j)
		{
			beta_g_r0[j].value = beta_g_r0[j].value * one_minus_r_0[i].value % prime_field::mod;
			beta_g_r1[j].value = beta_g_r1[j].value * one_minus_r_1[i].value % prime_field::mod;
		}
	}
	beta_u[0] = prime_field::field_element(1);
	beta_v[0] = prime_field::field_element(1);
	for(int i = 0; i < C.blocks[0].circuit[depth - 1].bit_length; ++i)
	{
		for(int j = 0; j < (1 << i); ++j)
		{
			beta_u[j | (1 << i)] = beta_u[j] * r_u[i];
			beta_v[j | (1 << i)] = beta_v[j] * r_v[i];
		}
			
		for(int j = 0; j < (1 << i); ++j)
		{
			beta_u[j] = beta_u[j] * one_minus_r_u[i];
			beta_v[j] = beta_v[j] * one_minus_r_v[i];
		}
	}
}

prime_field::field_element* generate_randomness(unsigned int size)
{
	int k = size;
	prime_field::field_element* ret;
	ret = new prime_field::field_element[k];

	for(int i = 0; i < k; ++i)
	{
		ret[i] = prime_field::random();
		ret[i].value = ret[i].value % prime_field::mod;
	}
	return ret;
}

prime_field::field_element verifier::V_in(const prime_field::field_element* r_b, const prime_field::field_element* one_minus_r_b,
										  const prime_field::field_element* r_g, const prime_field::field_element* one_minus_r_g,
								prime_field::field_element* output_raw, int r_b_size, int r_g_size,int output_size)
{
	prime_field::field_element* output = new prime_field::field_element[output_size];
	//output are arranged in blocks
	for(int i = 0; i < output_size; ++i)
		output[i] = output_raw[i];
	for(int i = 0; i < r_g_size; ++i)
	{
		int last_gate;
		int cnt = 0;
		for(int j = 0; j < (output_size >> 1); ++j)
			output[j] = output[j << 1] * (one_minus_r_g[i]) + output[j << 1 | 1] * (r_g[i]);
		output_size >>= 1;
	}
	for(int i = 0; i < r_b_size; ++i)
	{
		int last_gate;
		int cnt = 0;
		for(int j = 0; j < (output_size >> 1); ++j)
			output[j] = output[j << 1] * one_minus_r_b[i] * output[j << 1 | 1] * r_b[i];
		output_size >>= 1;
	}
	auto ret = output[0];
	ret.value = ret.value % prime_field::mod;
	delete[] output;
	if(ret.value < 0)
		ret.value = ret.value + prime_field::mod;
	return ret;
}

bool verifier::verify()
{
	prime_field::init_random();
	p -> proof_init();

	auto result = p -> evaluate();
//	for(int i = 0; i < (1 << C.circuit[C.total_depth - 1].bit_length); ++i)
//	{
//		fprintf(stderr, "%d %s\n", i, result[i].to_string(10).c_str());
//	}
//	fprintf(stderr, "\n");

	prime_field::field_element alpha, beta;
	alpha.value = 1;
	beta.value = 0;
	random_oracle oracle;
	//initial random value
	prime_field::field_element *r_0 = generate_randomness(C.blocks[0].circuit[C.blocks[0].total_depth - 1].bit_length), 
							   *r_1 = generate_randomness(C.blocks[0].circuit[C.blocks[0].total_depth - 1].bit_length);
	prime_field::field_element *r_b_0 = generate_randomness(C.total_blocks_binary_length);
	prime_field::field_element *one_minus_r_0, *one_minus_r_1, *one_minus_r_b_0;
	one_minus_r_0 = new prime_field::field_element[C.blocks[0].circuit[C.blocks[0].total_depth - 1].bit_length];
	one_minus_r_1 = new prime_field::field_element[C.blocks[0].circuit[C.blocks[0].total_depth - 1].bit_length];
	one_minus_r_b_0 = new prime_field::field_element[C.total_blocks_binary_length];

	for(int i = 0; i < (C.blocks[0].circuit[C.blocks[0].total_depth - 1].bit_length); ++i)
	{
		one_minus_r_0[i] = prime_field::field_element(1) - r_0[i];
		one_minus_r_1[i] = prime_field::field_element(1) - r_1[i];
	}
	for(int i = 0; i < C.total_blocks_binary_length; ++i)
	{
		one_minus_r_b_0[i] = prime_field::field_element(1) - r_b_0[i];
	}
	
	std::chrono::high_resolution_clock::time_point t_a = std::chrono::high_resolution_clock::now();
	std::cerr << "Calc V_output(r)" << std::endl;
	prime_field::field_element a_0 = p -> V_res(one_minus_r_0, r_0, one_minus_r_b_0, r_b_0, result, C.blocks[0].circuit[C.blocks[0].total_depth - 1].bit_length, C.total_blocks_binary_length, (1 << (C.blocks[0].circuit[C.blocks[0].total_depth - 1].bit_length)));
	delete[] result;
	std::chrono::high_resolution_clock::time_point t_b = std::chrono::high_resolution_clock::now();

	std::chrono::duration<double> ts = std::chrono::duration_cast<std::chrono::duration<double>>(t_b - t_a);
	std::cerr << "	Time: " << ts.count() << std::endl;
	a_0 = alpha * a_0;
	prime_field::field_element a_1 = prime_field::field_element(0); //* beta

	prime_field::field_element alpha_beta_sum = a_0; //+ a_1

	for(int i = C.blocks[0].total_depth - 1; i >= 1; --i)
	{
		std::chrono::high_resolution_clock::time_point t0 = std::chrono::high_resolution_clock::now();
		p -> sumcheck_init(i, C.total_blocks_binary_length, C.blocks[0].circuit[i].bit_length, C.blocks[0].circuit[i - 1].bit_length, C.blocks[0].circuit[i - 1].bit_length, alpha, beta, r_b_0, r_0, r_1, one_minus_r_b_0, one_minus_r_0, one_minus_r_1);
		std::cerr << "Bound blk index start" << std::endl;
		prime_field::field_element previous_random = prime_field::field_element(0);
		auto r_b = generate_randomness(C.total_blocks_binary_length);
		prime_field::field_element *one_minus_r_b;
		one_minus_r_b = new prime_field::field_element[C.total_blocks_binary_length];
		for(int j = 0; j < C.total_blocks_binary_length; ++j)
			one_minus_r_b[j] = prime_field::field_element(1) - r_b[j];

		p -> sumcheck_phase0_init();
		for(int j = 0; j < C.total_blocks_binary_length; ++j)
		{
			cubic_poly poly = p -> sumcheck_phase0_update(previous_random, j);
			previous_random = r_b[j];
			if(poly.eval(0) + poly.eval(1) != alpha_beta_sum)
			{
				fprintf(stderr, "Verification fail, phase0, circuit %d, current bit %d\n", i, j);
				return false;
			}
			else
			{

			}
			alpha_beta_sum = poly.eval(r_b[j]);
		}

		std::cerr << "Bound u start" << std::endl;
		p -> sumcheck_phase1_init();
		//next level random
		auto r_u = generate_randomness(C.blocks[0].circuit[i - 1].bit_length);
		auto r_v = generate_randomness(C.blocks[0].circuit[i - 1].bit_length);
		prime_field::field_element *one_minus_r_u, *one_minus_r_v;
		one_minus_r_u = new prime_field::field_element[C.blocks[0].circuit[i - 1].bit_length];
		one_minus_r_v = new prime_field::field_element[C.blocks[0].circuit[i - 1].bit_length];
		
		for(int j = 0; j < C.blocks[0].circuit[i - 1].bit_length; ++j)
		{
			one_minus_r_u[j] = prime_field::field_element(1) - r_u[j];
			one_minus_r_v[j] = prime_field::field_element(1) - r_v[j];
		}
		for(int j = 0; j < C.blocks[0].circuit[i - 1].bit_length; ++j)
		{
			quadratic_poly poly = p -> sumcheck_phase1_update(previous_random, j);
			previous_random = r_u[j];
			if(poly.eval(0) + poly.eval(1) != alpha_beta_sum)
			{
				fprintf(stderr, "Verification fail, phase1, circuit %d, current bit %d\n", i, j);
				return false;
			}
			else
			{
			//	fprintf(stderr, "Verification Pass, phase1, circuit %d, current bit %d\n", i, j);
			}
			alpha_beta_sum = poly.eval(r_u[j]);
		//	fprintf(stderr, "r_u[%d] = %s\n", j, r_u[j].to_string());
		//	fprintf(stderr, "alpha_beta_sum = %s\n", alpha_beta_sum.to_string());
		//	fprintf(stderr, "0_val = %s\n", poly.eval(prime_field::field_element(0)).to_string());
		//	fprintf(stderr, "1_val = %s\n", poly.eval(prime_field::field_element(1)).to_string());
		}
		std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();

		std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t1 - t0);
		std::cerr << "	Time: " << time_span.count() << std::endl;
		
		std::cerr << "Bound v start" << std::endl;
		t0 = std::chrono::high_resolution_clock::now();
		p -> sumcheck_phase2_init(previous_random, r_u, one_minus_r_u);
		previous_random = prime_field::field_element(0);
		for(int j = 0; j < C.blocks[0].circuit[i - 1].bit_length; ++j)
		{
			quadratic_poly poly = p -> sumcheck_phase2_update(previous_random, j);
			previous_random = r_v[j];
			if(poly.eval(0) + poly.eval(1) != alpha_beta_sum)
			{
				fprintf(stderr, "Verification fail, phase2, circuit level %d, current bit %d\n", i, j);
				return false;
			}
			else
			{
			//	fprintf(stderr, "Verification Pass, phase2, circuit level %d, current bit %d\n", i, j);
			}
			alpha_beta_sum = poly.eval(r_v[j]);
		}
		t1 = std::chrono::high_resolution_clock::now();
		time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t1 - t0);
		std::cerr << "	Time: " << time_span.count() << std::endl;
		
		auto final_claims = p -> sumcheck_finalize(previous_random);
		auto v_u = final_claims.first;
		auto v_v = final_claims.second;

//		std::cout << "v_u = " << v_u.to_string(10) << std::endl;
//		std::cout << "v_v = " << v_v.to_string(10) << std::endl;

//		std::cout << "alpha = " << alpha.to_string(10) << std::endl;
//		std::cout << "beta = " << beta.to_string(10) << std::endl;
		beta_init(i, alpha, beta, r_0, r_1, r_u, r_v, one_minus_r_0, one_minus_r_1, one_minus_r_u, one_minus_r_v);
		auto mult_value = mult(i);
		auto add_value = add(i);
//		std::cout << "mult_value = " << mult_value.to_string(10) << std::endl;
//		std::cout << "add_value = " << add_value.to_string(10) << std::endl;

		if(alpha_beta_sum != add_value * (v_u + v_v) + mult_value * v_u * v_v)
		{
			fprintf(stderr, "Verification fail, semi final, circuit level %d\n", i);
			return false;
		}
		else
		{
			fprintf(stderr, "Verification Pass, semi final, circuit level %d\n", i);
		}
		auto tmp_alpha = generate_randomness(1), tmp_beta = generate_randomness(1);
		alpha = tmp_alpha[0];
		beta = tmp_beta[0];
		delete[] tmp_alpha;
		delete[] tmp_beta;
		alpha_beta_sum = alpha * v_u + beta * v_v;

		delete[] r_0;
		delete[] r_1;
		delete[] r_b_0;
		delete[] one_minus_r_b_0;
		delete[] one_minus_r_0;
		delete[] one_minus_r_1;
		r_0 = r_u;
		r_1 = r_v;
		r_b_0 = r_b;
		one_minus_r_0 = one_minus_r_u;
		one_minus_r_1 = one_minus_r_v;
		one_minus_r_b_0 = one_minus_r_b;
		std::cerr << "Prove Time " << p -> total_time << std::endl;
	}

	//post sumcheck
	prime_field::field_element* input;
	input = new prime_field::field_element[(1 << C.blocks[0].circuit[0].bit_length) * C.total_blocks];

	for(int blk = 0; blk < C.total_blocks; ++blk)
	{
		int blk_size = (1 << C.blocks[blk].circuit[0].bit_length);
		for(int i = 0; i < (1 << C.blocks[blk].circuit[0].bit_length); ++i)
		{
			int g = blk * blk_size + i;
			if(C.blocks[blk].circuit[0].gates[i].ty == 3)
			{
				input[g] = prime_field::field_element(C.blocks[blk].circuit[0].gates[i].u);
			}
			else
				assert(false);
		}
	}
	auto input_0 = V_in(r_b_0, one_minus_r_b_0, r_0, one_minus_r_0, input, C.total_blocks_binary_length, C.blocks[0].circuit[0].bit_length, (1 << C.blocks[0].circuit[0].bit_length)), 
		 input_1 = V_in(r_b_0, one_minus_r_b_0, r_1, one_minus_r_1, input, C.total_blocks_binary_length, C.blocks[0].circuit[0].bit_length, (1 << C.blocks[0].circuit[0].bit_length));

	delete[] input;
	delete[] r_0;
	delete[] r_1;
	delete[] one_minus_r_0;
	delete[] one_minus_r_1;
	if(alpha_beta_sum != input_0 * alpha + input_1 * beta)
	{
		fprintf(stderr, "Verification fail, final input check fail.\n");
		return false;
	}
	else
	{
		fprintf(stderr, "Verification pass\n");
		std::cerr << "Prove Time " << p -> total_time << std::endl;
	}
	p -> delete_self();
	delete_self();
	return true;
}

void verifier::delete_self()
{
	delete[] beta_g_r0;
	delete[] beta_g_r1;
	delete[] beta_u;
	delete[] beta_v;
	for(int b = 0; b < C.total_blocks; ++b)
	{
		for(int i = 0; i < C.blocks[b].total_depth; ++i)
		{
			delete[] C.blocks[b].circuit[i].gates;
		}
		delete[] C.blocks[b].circuit;
	}
	delete[] C.blocks;
}
