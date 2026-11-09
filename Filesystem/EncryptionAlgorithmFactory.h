#pragma once

#include "EncryptionAlgorithm.h"

#include <memory>

class EncryptionAlgorithmFactory
{
public:
	enum class Algorithm { Aes };

	EncryptionAlgorithmFactory(Algorithm algorithm, std::unique_ptr<EncryptionAlgorithmParameters> parameters);
	EncryptionAlgorithmFactory(EncryptionAlgorithmFactory&&) noexcept = default;
	EncryptionAlgorithmFactory& operator=(EncryptionAlgorithmFactory&&) noexcept = default;
	EncryptionAlgorithmFactory(const EncryptionAlgorithmFactory&) = delete;
	EncryptionAlgorithmFactory& operator=(const EncryptionAlgorithmFactory&) = delete;

	Algorithm GetAlgorithm() const;
	QByteArray GenerateKey() const;
	std::unique_ptr<EncryptionAlgorithm> Create(EncryptionAlgorithm::Mode mode, const QByteArray& key) const;

private:
	Algorithm algorithm;
	std::unique_ptr<EncryptionAlgorithmParameters> parameters;
};
