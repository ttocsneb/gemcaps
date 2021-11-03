pub struct ResolvesServerCertUsingSni {
    by_name: collections::HashMap<String, Arc<sign::CertifiedKey>>,
}

impl ResolvesServerCertUsingSni {
    /// Create a new and empty (i.e., knows no certificates) resolver.
    pub fn new() -> Self {
        Self {
            by_name: collections::HashMap::new(),
        }
    }

    /// Add a new `sign::CertifiedKey` to be used for the given SNI `name`.
    ///
    /// This function fails if `name` is not a valid DNS name, or if
    /// it's not valid for the supplied certificate, or if the certificate
    /// chain is syntactically faulty.
    pub fn add(&mut self, name: &str, ck: sign::CertifiedKey) -> Result<(), Error> {
        let checked_name = webpki::DnsNameRef::try_from_ascii_str(name)
            .map_err(|_| Error::General("Bad DNS name".into()))?;

        ck.cross_check_end_entity_cert(Some(checked_name))?;
        self.by_name
            .insert(name.into(), Arc::new(ck));
        Ok(())
    }
}

impl server::ResolvesServerCert for ResolvesServerCertUsingSni {
    fn resolve(&self, client_hello: ClientHello) -> Option<Arc<sign::CertifiedKey>> {
        if let Some(name) = client_hello.server_name() {
            self.by_name.get(name).map(Arc::clone)
        } else {
            // This kind of resolver requires SNI
            None
        }
    }
}