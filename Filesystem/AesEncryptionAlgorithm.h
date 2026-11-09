#pragma once

#include "EncryptionAlgorithm.h"

typedef struct evp_cipher_ctx_st EVP_CIPHER_CTX;

struct AesEncryptionAlgorithmParameters : EncryptionAlgorithmParameters
{
	enum class Cipher { Aes256Gcm };

	AesEncryptionAlgorithmParameters(Cipher cipher, int nonceSize, int tagSize);

	Cipher cipher;
	int nonceSize;
	int tagSize;
};

class AesEncryptionAlgorithm : public EncryptionAlgorithm
{
public:
	AesEncryptionAlgorithm(Mode mode, const EncryptionAlgorithmParameters& parameters, const QByteArray& key);
	~AesEncryptionAlgorithm() override;
	AesEncryptionAlgorithm(const AesEncryptionAlgorithm&) = delete;
	AesEncryptionAlgorithm& operator=(const AesEncryptionAlgorithm&) = delete;

	static QByteArray GenerateKey(const EncryptionAlgorithmParameters& parameters);
	QByteArray Update(const QByteArray& input) override;
	QByteArray Finish() override;

private:
	void InitializeDecryption();
	static const AesEncryptionAlgorithmParameters& GetParameters(const EncryptionAlgorithmParameters& parameters);

	EVP_CIPHER_CTX* context{};
	Mode mode;
	QByteArray key;
	QByteArray pending;
	AesEncryptionAlgorithmParameters parameters;
	bool finished{};
	bool headerWritten{};
};
