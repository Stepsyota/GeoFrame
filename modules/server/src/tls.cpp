#include "server/tls.hpp"

#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace geoframe::server {

namespace {

void throw_openssl(const std::string& context) {
    throw std::runtime_error("TLS cert generation failed at: " + context);
}

}  // namespace

void ensure_self_signed_cert(const std::filesystem::path& cert_path,
                             const std::filesystem::path& key_path) {
    if (std::filesystem::exists(cert_path) && std::filesystem::exists(key_path)) {
        return;  // already generated
    }

    std::filesystem::create_directories(cert_path.parent_path());

    // ── Generate RSA-2048 key ─────────────────────────────────────────────
    EVP_PKEY* pkey = EVP_RSA_gen(2048);
    if (pkey == nullptr) {
        throw_openssl("EVP_RSA_gen");
    }

    // ── Create X.509 certificate ─────────────────────────────────────────
    X509* x509 = X509_new();
    if (x509 == nullptr) {
        EVP_PKEY_free(pkey);
        throw_openssl("X509_new");
    }

    X509_set_version(x509, 2);  // v3

    // Serial number: 1
    ASN1_INTEGER_set(X509_get_serialNumber(x509), 1);

    // Valid for 10 years
    X509_gmtime_adj(X509_get_notBefore(x509), 0);
    X509_gmtime_adj(X509_get_notAfter(x509), 10L * 365 * 24 * 60 * 60);

    X509_set_pubkey(x509, pkey);

    // Subject / issuer: CN=GeoFrame
    X509_NAME* name = X509_get_subject_name(x509);
    X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC,
                               reinterpret_cast<const unsigned char*>("GeoFrame"), -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                               reinterpret_cast<const unsigned char*>("GeoFrame"), -1, -1, 0);
    X509_set_issuer_name(x509, name);

    // Sign with SHA-256
    if (X509_sign(x509, pkey, EVP_sha256()) == 0) {
        X509_free(x509);
        EVP_PKEY_free(pkey);
        throw_openssl("X509_sign");
    }

    // ── Write cert.pem ───────────────────────────────────────────────────
    {
        std::FILE* fp = std::fopen(cert_path.string().c_str(), "w");
        if (fp == nullptr) {
            X509_free(x509);
            EVP_PKEY_free(pkey);
            throw std::runtime_error("Cannot open " + cert_path.string() + " for writing");
        }
        PEM_write_X509(fp, x509);
        std::fclose(fp);
    }

    // ── Write key.pem ────────────────────────────────────────────────────
    {
        std::FILE* fp = std::fopen(key_path.string().c_str(), "w");
        if (fp == nullptr) {
            X509_free(x509);
            EVP_PKEY_free(pkey);
            throw std::runtime_error("Cannot open " + key_path.string() + " for writing");
        }
        PEM_write_PrivateKey(fp, pkey, nullptr, nullptr, 0, nullptr, nullptr);
        std::fclose(fp);
    }

    X509_free(x509);
    EVP_PKEY_free(pkey);
}

}  // namespace geoframe::server
