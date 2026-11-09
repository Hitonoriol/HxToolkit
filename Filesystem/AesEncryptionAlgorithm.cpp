#include "AesEncryptionAlgorithm.h"

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <stdexcept>

AesEncryptionAlgorithmParameters::AesEncryptionAlgorithmParameters(Cipher value, int ivSize, int authenticationTagSize)
	: cipher(value)
	, nonceSize(ivSize)
	, tagSize(authenticationTagSize)
{
}

AesEncryptionAlgorithm::AesEncryptionAlgorithm(Mode value, const EncryptionAlgorithmParameters& configuration,
	const QByteArray& encryptionKey)
	: mode(value)
	, key(encryptionKey)
	, parameters(GetParameters(configuration))
{
	if (parameters.cipher != AesEncryptionAlgorithmParameters::Cipher::Aes256Gcm || parameters.nonceSize <= 0
		|| parameters.tagSize <= 0 || parameters.tagSize > 16 || key.size() != 32) {
		throw std::invalid_argument("The AES encryption parameters are invalid.");
	}

	context = EVP_CIPHER_CTX_new();
	if (!context) {
		throw std::runtime_error("Could not initialize AES encryption.");
	}

	if (mode == Mode::Decrypt) {
		return;
	}

	pending.resize(parameters.nonceSize);
	if (RAND_bytes(reinterpret_cast<unsigned char*>(pending.data()), pending.size()) != 1
		|| EVP_EncryptInit_ex(context, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1
		|| EVP_CIPHER_CTX_ctrl(context, EVP_CTRL_GCM_SET_IVLEN, pending.size(), nullptr) != 1
		|| EVP_EncryptInit_ex(context, nullptr, nullptr,
			reinterpret_cast<const unsigned char*>(key.constData()),
			reinterpret_cast<const unsigned char*>(pending.constData())) != 1) {
		EVP_CIPHER_CTX_free(context);
		context = nullptr;
		throw std::runtime_error("Could not initialize AES encryption.");
	}
}

AesEncryptionAlgorithm::~AesEncryptionAlgorithm()
{
	EVP_CIPHER_CTX_free(context);
}

QByteArray AesEncryptionAlgorithm::GenerateKey(const EncryptionAlgorithmParameters& configuration)
{
	const auto& parameters = GetParameters(configuration);
	if (parameters.cipher != AesEncryptionAlgorithmParameters::Cipher::Aes256Gcm) {
		throw std::invalid_argument("The AES cipher is not supported.");
	}

	QByteArray value(32, Qt::Uninitialized);
	if (RAND_bytes(reinterpret_cast<unsigned char*>(value.data()), value.size()) != 1) {
		throw std::runtime_error("Could not generate the AES encryption key.");
	}

	return value;
}

QByteArray AesEncryptionAlgorithm::Update(const QByteArray& input)
{
	if (finished) {
		throw std::logic_error("Encryption has already finished.");
	}

	QByteArray output;
	if (mode == Mode::Decrypt && !headerWritten) {
		pending.append(input);
		if (pending.size() < 4 + parameters.nonceSize + parameters.tagSize) {
			return output;
		}

		InitializeDecryption();
	} else if (mode == Mode::Decrypt) {
		pending.append(input);
	} else {
		if (!headerWritten) {
			output = QByteArray("FC02", 4) + pending;
			headerWritten = true;
		}

		const auto offset = output.size();
		output.resize(offset + input.size() + EVP_MAX_BLOCK_LENGTH);
		int written = 0;
		if (EVP_EncryptUpdate(context, reinterpret_cast<unsigned char*>(output.data()) + offset, &written,
			reinterpret_cast<const unsigned char*>(input.constData()), input.size()) != 1) {
			throw std::runtime_error("Could not encrypt the chunk.");
		}

		output.resize(offset + written);
		return output;
	}

	const auto cipherSize = pending.size() - parameters.tagSize;
	if (cipherSize <= 0) {
		return output;
	}

	output.resize(cipherSize + EVP_MAX_BLOCK_LENGTH);
	int written = 0;
	if (EVP_DecryptUpdate(context, reinterpret_cast<unsigned char*>(output.data()), &written,
		reinterpret_cast<const unsigned char*>(pending.constData()), cipherSize) != 1) {
		throw std::runtime_error("Could not decrypt the chunk.");
	}

	output.resize(written);
	pending.remove(0, cipherSize);
	return output;
}

QByteArray AesEncryptionAlgorithm::Finish()
{
	if (finished) {
		throw std::logic_error("Encryption has already finished.");
	}

	QByteArray output;
	if (mode == Mode::Encrypt) {
		if (!headerWritten) {
			output = QByteArray("FC02", 4) + pending;
			headerWritten = true;
		}

		const auto offset = output.size();
		output.resize(offset + EVP_MAX_BLOCK_LENGTH + parameters.tagSize);
		int written = 0;
		if (EVP_EncryptFinal_ex(context, reinterpret_cast<unsigned char*>(output.data()) + offset, &written) != 1
			|| EVP_CIPHER_CTX_ctrl(context, EVP_CTRL_GCM_GET_TAG, parameters.tagSize,
				output.data() + offset + written) != 1) {
			throw std::runtime_error("Could not encrypt the chunk.");
		}

		output.resize(offset + written + parameters.tagSize);
		finished = true;
		return output;
	}

	if (!headerWritten || pending.size() != parameters.tagSize
		|| EVP_CIPHER_CTX_ctrl(context, EVP_CTRL_GCM_SET_TAG, parameters.tagSize, pending.data()) != 1) {
		throw std::runtime_error("The downloaded chunk format is invalid.");
	}

	output.resize(EVP_MAX_BLOCK_LENGTH);
	int written = 0;
	if (EVP_DecryptFinal_ex(context, reinterpret_cast<unsigned char*>(output.data()), &written) != 1) {
		throw std::runtime_error("Could not decrypt the chunk. Check the key and file integrity.");
	}

	output.resize(written);
	finished = true;
	return output;
}

void AesEncryptionAlgorithm::InitializeDecryption()
{
	if (pending.first(4) != "FC02") {
		throw std::runtime_error("The downloaded chunk format is invalid.");
	}

	const auto nonce = pending.sliced(4, parameters.nonceSize);
	pending.remove(0, 4 + parameters.nonceSize);
	if (EVP_DecryptInit_ex(context, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1
		|| EVP_CIPHER_CTX_ctrl(context, EVP_CTRL_GCM_SET_IVLEN, nonce.size(), nullptr) != 1
		|| EVP_DecryptInit_ex(context, nullptr, nullptr,
			reinterpret_cast<const unsigned char*>(key.constData()),
			reinterpret_cast<const unsigned char*>(nonce.constData())) != 1) {
		throw std::runtime_error("Could not initialize AES decryption.");
	}

	headerWritten = true;
}

const AesEncryptionAlgorithmParameters& AesEncryptionAlgorithm::GetParameters(
	const EncryptionAlgorithmParameters& parameters)
{
	const auto value = dynamic_cast<const AesEncryptionAlgorithmParameters*>(&parameters);
	if (!value) {
		throw std::invalid_argument("AES encryption parameters are required.");
	}

	return *value;
}
