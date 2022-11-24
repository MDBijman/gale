use proc_macro::TokenStream;
use quote::quote;
use syn;

/*
* We provide some utilities for the (bidirectional) mapping between rust data and textual gale bytecode.
*/

#[proc_macro_derive(FromTerm)]
pub fn derive_from_term(input: TokenStream) -> TokenStream {
    let ast: syn::DeriveInput = syn::parse(input).unwrap();
    let name = &ast.ident;

    let fields = match ast.data {
        syn::Data::Struct(s) => s.fields,
        syn::Data::Enum(_) => todo!(),
        syn::Data::Union(_) => todo!(),
    };

    let result = match fields {
        syn::Fields::Named(_) => {
            let field_inits = fields.into_iter().enumerate().map(|(idx, f)| {
                let field_idx = syn::Index::from(idx);
                let field_name = f.ident.unwrap();
                let field_type = f.ty;

                quote! {
                    #field_name: <#field_type>::construct(module_loader, module, &elements[#field_idx]).unwrap()
                }
            });

            quote! {
                Ok(#name {
                    #(#field_inits),*
                })
            }
        }
        syn::Fields::Unnamed(_) => {
            let field_inits = fields.into_iter().enumerate().map(|(idx, f)| {
                let field_idx = syn::Index::from(idx);
                let field_type = f.ty;

                quote! {
                    <#field_type>::construct(module_loader, module, &elements[#field_idx]).unwrap()
                }
            });

            quote! {
                Ok(#name (
                    #(#field_inits),*
                ))
            }
        }
        syn::Fields::Unit => panic!(),
    };

    let res = quote! {
        impl FromTerm for #name {
            fn construct(module_loader: &::vm_internal::bytecode::ModuleLoader, module: &mut ::vm_internal::bytecode::Module, t: &::vm_internal::parser::Term)
            -> Result<Self, ::vm_internal::dialect::InstructionParseError> {
                let p = match t {
                    Term::Instruction(i) => i,
                    _ => panic!("Term must be instruction")
                };
                let dialect = &p.dialect;
                let name = &p.name;
                let elements = &p.elements;
                #result
            }
        }
    };
    res.into()
}
