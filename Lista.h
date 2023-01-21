#ifndef _LISTA_HPP_
#define _LISTA_HPP_

namespace NPLista {
	template <typename TBase>
	class ClaseLista {
	private:
		struct Tnodo {
			TBase valor;
			Tnodo *sig;
		};

		typedef Tnodo *TipoLista;
		TipoLista lista;
		unsigned longitud;

		void insertarAlPrincipio(TipoLista &lista, const TBase &valor) {
			TipoLista aux;

			aux = new Tnodo;
			aux->valor = valor;
			aux->sig = lista;
			lista = aux;
		}

		void insertarAlFinal(TipoLista &lista, const TBase &valor) {
			TipoLista aux;

			if (lista == NULL) { // Lista vac�a
				this->insertarAlPrincipio(lista, valor);
			}
			else {
				aux = lista;
				while (aux->sig != NULL) {
					aux = aux->sig;
				}
				aux->sig = new Tnodo;
				aux = aux->sig;
				aux->valor = valor;
				aux->sig = NULL;
			}
		}

		void eliminarPrimero(TipoLista &lista) {
			TipoLista aux;

			if (lista != NULL) {
				aux = lista;
				lista = lista->sig;
				delete aux;
			}
		}

		void eliminarUltimo(TipoLista &lista) {
			TipoLista aux;

			if (lista != NULL) {
				if (lista->sig == NULL) {
					eliminarPrimero(lista);
				}
				else {
					aux = lista;
					while (aux->sig->sig != NULL) {
						aux = aux->sig;
					}
					delete aux->sig;
					aux->sig = NULL;
				}
			}
		}

		void borrarLista(TipoLista &lista) {
			while (lista != NULL) {
				this->eliminarPrimero(lista);
			}
		}

	public:
		ClaseLista() {
			lista = NULL;
			longitud = 0;
		}

		~ClaseLista() {
			borrarLista(lista);
		}

		bool vacia() const {
			return (longitud == 0);
		}

		unsigned obtenerLongitud() const {
			return longitud;
		}

		void insertar(const unsigned indice, const TBase &nuevoElemento, bool &exito) {
			TipoLista aux, nuevo;
			unsigned pos;

			exito = (indice >= 1) && (indice <= longitud + 1);
			if (exito) {
				if (indice == 1) {
					this->insertarAlPrincipio(lista, nuevoElemento);
				}
				else if (indice == longitud + 1) {
					this->insertarAlFinal(lista, nuevoElemento);
				}
				else {
					nuevo = new Tnodo;
					nuevo->valor = nuevoElemento;
					aux = lista;
					pos = 1;
					while (pos < indice - 1) {
						aux = aux->sig;
						++pos;
					}
					nuevo->sig = aux->sig;
					aux->sig = nuevo;
				}
				++longitud;
			}
		}

			void asignar(const unsigned indice, const TBase &nuevoElemento, bool &exito) {
				eliminar(indice, exito);
				insertar(indice, nuevoElemento, exito);
			}

			void eliminar(const unsigned indice, bool &exito) {
				TipoLista aux2, aux;
				unsigned pos;

				exito = (indice >= 1) && (indice <= longitud);
				if (exito) {
					if (indice == 1) {
						this->eliminarPrimero(lista);
					}
					else if (indice == longitud) {
						this->eliminarUltimo(lista);
					}
					else {
						aux2 = lista;
						pos = 1;
						while (pos < indice - 1) {
							aux2 = aux2->sig;
							++pos;
						}
						aux = aux2->sig;
						aux2->sig = aux->sig;
						delete aux;
					}
					--longitud;
				} // Fin if
			}

			void consultar(unsigned indice, TBase &elemento, bool &exito) const {
				TipoLista aux;
				unsigned pos;

				exito = (indice >= 1) && (indice <= longitud);

				if (exito) {
					aux = lista; pos = 1;
					while (pos < indice) {
						aux = aux->sig;
						++pos;
					}
					elemento = aux->valor;
				}
			} // Fin consultar

			void limpiar() {
				borrarLista(lista);
				longitud = 0;
			}
		};
	}
#endif